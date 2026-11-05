#!/usr/bin/env python3
"""Differential native SQLite-file, typed SQL, recovery and locking checks.

The SQLite dependency is test-only. This uses Python's reported SQLite version,
not a claim that the full pinned reference profile is implemented.
"""
import argparse
import os
from pathlib import Path
import sqlite3
import struct
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', required=True)
    args = parser.parse_args()
    engine = str(Path(args.engine).resolve())

    def native(mode, path, sql, success=True):
        proc = subprocess.run([engine, mode, str(path), sql], capture_output=True, text=True, timeout=30)
        if success:
            assert proc.returncode == 0, (sql, proc.stdout, proc.stderr)
        else:
            assert proc.returncode != 0, (sql, proc.stdout, proc.stderr)
        return proc

    def typed(value):
        if value is None:
            return 'N'
        if isinstance(value, int):
            return f'I{value}'
        if isinstance(value, float):
            return 'R' + format(value, '.17g')
        if isinstance(value, str):
            return 'T' + value.encode('utf-8').hex()
        return 'B' + value.hex()

    def compare(path, sql):
        with sqlite3.connect(path) as ref:
            expected = ['\t'.join(map(typed, row)) for row in ref.execute(sql)]
        actual = native('--query', path, sql).stdout.splitlines()
        assert actual == expected, (sql, actual, expected)

    parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='mulib-sqlite-', dir=parent) as directory:
        directory = Path(directory)
        path = directory / 'native.db'
        native('--exec', path, "CREATE TABLE data(id INTEGER PRIMARY KEY, name TEXT NOT NULL, amount NUMERIC, raw BLOB);"
               "INSERT INTO data VALUES(-9223372036854775808,'Ada',42,x'0001ff'),"
               "(9223372036854775807,'Grace',2.5,x''); PRAGMA user_version=27;")
        with sqlite3.connect(path) as ref:
            assert ref.execute('PRAGMA integrity_check').fetchall() == [('ok',)]
            assert ref.execute('PRAGMA user_version').fetchone() == (27,)
            ref.execute("INSERT INTO data(id,name,amount,raw) VALUES(3,?,?,?)", ('x\0y', '51', bytes(range(256)) * 40))
        for sql in ("SELECT * FROM data ORDER BY id", "SELECT count(*),sum(amount),min(id),max(id),avg(amount) FROM data",
                    "SELECT id FROM data WHERE amount>40 ORDER BY id", "SELECT name,typeof(amount),length(raw) FROM data ORDER BY id",
                    "SELECT DISTINCT amount FROM data ORDER BY amount LIMIT 2 OFFSET 1"):
            compare(path, sql)
        compare(path, 'SELECT name,max(id) FROM data')
        native('--exec', path, 'UPDATE data SET amount=amount+1 WHERE id=3;')
        compare(path, 'SELECT * FROM data ORDER BY id')
        compare(path, "SELECT typeof('1junk'+1), '1junk'+1, abs('1'), 'NaN' OR 0, typeof(1e999)")
        compare(path, "SELECT 'é' LIKE '_', 'hello🌍' LIKE '%🌍', 'a🌍b' LIKE 'a_b'")
        compare(path, 'PRAGMA table_info(data)')
        saved = path.read_bytes()
        native('--exec', path, "INSERT INTO data VALUES(9,'first',1,NULL),(3,'duplicate',2,NULL);", success=False)
        assert path.read_bytes() == saved
        with sqlite3.connect(path) as ref:
            assert ref.execute('PRAGMA integrity_check').fetchall() == [('ok',)]

        # Force leaf/interior and overflow pages using reference-created images.
        for ps in (512, 1024, 4096, 65536):
            file = directory / f'page-{ps}.db'
            with sqlite3.connect(file) as ref:
                ref.execute(f'PRAGMA page_size={ps}')
                ref.execute('CREATE TABLE t(id INTEGER PRIMARY KEY, value TEXT, payload BLOB)')
                ref.executemany('INSERT INTO t VALUES(?,?,?)', [(i - 50, f'row-{i}', bytes([i % 256]) * (7000 if i % 17 == 0 else 5)) for i in range(400)])
            compare(file, 'SELECT id,length(payload) FROM t ORDER BY id DESC LIMIT 7')
            compare(file, 'PRAGMA table_info(t)')
            native('--exec', file, "UPDATE t SET value='updated' WHERE id<0;")
            with sqlite3.connect(file) as ref:
                assert ref.execute('PRAGMA integrity_check').fetchall() == [('ok',)]
                assert ref.execute("SELECT count(*) FROM t WHERE value='updated'").fetchone() == (50,)
                assert ref.execute('PRAGMA page_size').fetchone() == (ps,)

        # Unsupported dependencies must prevent writes without altering bytes.
        with sqlite3.connect(path) as ref:
            ref.execute('CREATE INDEX data_amount ON data(amount)')
        compare(path, 'SELECT id FROM data WHERE amount>40 ORDER BY id')
        original = path.read_bytes()
        native('--exec', path, "DELETE FROM data;", success=False)
        assert path.read_bytes() == original

        # Native writer conflicts with a real SQLite lock holder.
        with sqlite3.connect(path) as ref:
            ref.execute('BEGIN EXCLUSIVE')
            native('--query', path, 'SELECT 1', success=False)
            ref.rollback()
        holder = subprocess.Popen([engine, '--hold', str(path)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        try:
            assert holder.stdout.readline().strip() == 'locked'
            with sqlite3.connect(path, timeout=0) as ref:
                try:
                    ref.execute('SELECT * FROM data').fetchall()
                    raise AssertionError('reference read ignored native exclusive lock')
                except sqlite3.OperationalError as error:
                    assert 'locked' in str(error)
        finally:
            holder.communicate('\n', timeout=10)
            assert holder.returncode == 0

        # Build a compatible hot rollback journal, then damage the main header.
        # Each engine independently recovers an identical fixture.
        for reference in (False, True):
            recovered = directory / f'recover-{reference}.db'
            recovered.write_bytes(original)
            ps = struct.unpack('>H', original[16:18])[0]
            if ps == 1:
                ps = 65536
            pages = len(original) // ps
            nonce = 0x73193519
            header = bytes.fromhex('d9d505f920a163d7') + struct.pack('>IIIII', pages, nonce, pages, 512, ps)
            journal = bytearray(header.ljust(512, b'\0'))
            for page in range(pages):
                data = original[page * ps:(page + 1) * ps]
                checksum = (nonce + sum(data[i] for i in range(ps - 200, -1, -200))) & 0xffffffff
                journal += struct.pack('>I', page + 1) + data + struct.pack('>I', checksum)
            Path(str(recovered) + '-journal').write_bytes(journal)
            broken = bytearray(original)
            broken[0:16] = b'broken file!!!!!'
            recovered.write_bytes(broken)
            if reference:
                with sqlite3.connect(recovered) as ref:
                    assert ref.execute('PRAGMA integrity_check').fetchall() == [('ok',)]
            else:
                native('--readonly', recovered, 'SELECT 1', success=False)
                assert recovered.read_bytes() == broken
                native('--query', recovered, 'SELECT count(*) FROM data')
            assert recovered.read_bytes() == original
            assert not Path(str(recovered) + '-journal').exists()

        wal = directory / 'wal.db'
        with sqlite3.connect(wal) as ref:
            ref.execute('PRAGMA journal_mode=WAL')
            ref.execute('CREATE TABLE t(x)')
            ref.commit()
            native('--query', wal, 'SELECT * FROM t', success=False)

    print(f'SQLite interoperability passed against Python SQLite {sqlite3.sqlite_version}; not full compatibility certification')


if __name__ == '__main__':
    main()
