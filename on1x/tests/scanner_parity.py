#!/usr/bin/env python3
"""Compare exact token bytes, offsets and diagnostics from both scanners."""
from pathlib import Path
import random
import subprocess
import sys

generated, reference, examples, work = map(Path, sys.argv[1:5])
work.mkdir(parents=True, exist_ok=True)
count = 0


def probe(tool, path):
    return subprocess.run([str(tool), str(path)], capture_output=True, timeout=30)


def check(name, data, *, invalid_at=None, diagnostic='invalid token', valid=False):
    global count
    path = work / (name + '.on1x')
    path.write_bytes(data)
    r, g = probe(reference, path), probe(generated, path)
    assert g.returncode in (0, 1), (name, 'generated scanner crashed', g.returncode, g.stderr)
    assert r.returncode in (0, 1), (name, 'fallback scanner crashed', r.returncode, r.stderr)
    assert (r.returncode, r.stdout, r.stderr) == (g.returncode, g.stdout, g.stderr), (
        name, data[:200], r.returncode, r.stdout[:500], r.stderr,
        g.returncode, g.stdout[:500], g.stderr)
    if invalid_at is not None:
        line = data.count(b'\n', 0, invalid_at) + 1
        column = invalid_at - data.rfind(b'\n', 0, invalid_at)
        expected = f'{invalid_at}:{line}:{column}: error: {diagnostic}\n'.encode()
        assert g.returncode == 1 and g.stderr == expected, (name, g.stderr, expected)
        assert not g.stdout, (name, 'partial tokens exposed on failure')
    elif valid:
        assert g.returncode == 0, (name, g.stderr)
    if g.returncode == 0:
        tokens = g.stdout.splitlines()
        assert tokens, (name, 'missing EOF')
        previous_end = 0
        for i, token in enumerate(tokens):
            coordinates, text = token.split(b' ')
            offset, line, column = map(int, coordinates.split(b':'))
            text = bytes.fromhex(text.decode('ascii'))
            assert line == data.count(b'\n', 0, offset) + 1, (name, token)
            assert column == offset - data.rfind(b'\n', 0, offset), (name, token)
            assert previous_end <= offset <= len(data), (name, token)
            if i == len(tokens) - 1:
                assert offset == len(data) and text == b'<eof>', (name, token)
            else:
                assert data[offset:offset + len(text)] == text, (name, token)
                previous_end = offset + len(text)
    count += 1


for source in sorted(examples.glob('*.on1x')):
    check('example_' + source.stem, source.read_bytes(), valid=True)

valid_cases = [
    b'', b'#', b' \t\r\n# comment\r\n# final',
    b'abcdefghijklmnopqrstuvwxyz_ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 00019',
    b'== != <= >= && || -> + - * / % < > = ! ; : , ( ) { } [ ]',
    b'@ppx foo "path" macro rule $name ... => 0x2a 0XFF 0x 0xG 0x1z ...',
    b'macro m literals(else,_) ellipsis dots { rule ([$x:Syntax dots . $rest:Syntax]) => block { f($x) dots; }; }',
    b'quote { splice(code) + lift(value) } run(code) code<code<i64>>=value vector[1,2] . .. ... ....',
    b'===!==<==>==&&||- ->> ///',
    b'"# not a comment"# comment\n"snow: \xe9\x9b\xaa" name',
    b'"\\\\ \\" \\n \\r \\t \\0"',
    b'"raw\tbyte\x01\xff"',
    b'# \xff invalid UTF-8 in comment\r\nfn main() {}',
    b'x# inline\r\n +# more\n1',
    b'name+12;\n' * 4096,
    b'"' + b'x' * 65536 + b'"',
    b'#' + b'x' * (2 * 1024 * 1024 - 1),
]
for i, data in enumerate(valid_cases):
    check(f'valid_{i}', data, valid=True)

prefix = b'# layout\r\n\t"snow: \xe9\x9b\xaa" ; '
for i, bad in enumerate([b'`', b'&', b'|', b'\v', b'\f', b'\xff', b'"unclosed',
                         b'"bad\\q"', b'"bad\\', b'"raw\nnewline"', b'"raw\rnewline"']):
    check(f'invalid_{i}', prefix + bad, invalid_at=len(prefix))
for i, data in enumerate([b'\0', b'# comment\0', b'"string\0"', b'@\n\0']):
    check(f'nul_{i}', data, invalid_at=data.index(b'\0'), diagnostic='NUL in source')
check('oversized', b'#' * (2 * 1024 * 1024 + 1), invalid_at=0,
      diagnostic='source exceeds 2 MiB limit')

rng = random.Random(20261114)
spellings = [b'name', b'_1', b'019', b'"text#\\n"', b'"\xe9\x9b\xaa"', b'<=', b'->', b'/', b'[]', b'!=']
layouts = [b' ', b'\t', b'\r\n', b'# comment\n']
for i in range(40):
    data = b''.join(rng.choice(spellings) + rng.choice(layouts) for _ in range(32))
    check(f'random_valid_{i}', data, valid=True)
alphabet = b'abc_01234 +-*/%!=<>&|#\\"\n\r\t@\x7f\x80'
for i in range(160):
    data = bytes(rng.choice(alphabet) for _ in range(rng.randrange(150)))
    check(f'random_bytes_{i}', data)
print(f'Aurocks/fallback parity: {count} exact token/diagnostic cases PASS')
