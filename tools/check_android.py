#!/usr/bin/env python3
"""Independent DEX integrity/layout, JVM fixture, C AOT and native format checks.

No Android SDK or Python packages required. readelf/llvm-readobj are optional
independent format readers; this does not claim on-device ART/NDK conformance.
"""
import argparse
import hashlib
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import tempfile
import zlib


def run(command):
    return subprocess.run(command, check=True, text=True, capture_output=True, timeout=90).stdout


def dex_check(path):
    p = path.read_bytes()
    u16 = lambda at: struct.unpack_from('<H', p, at)[0]
    u32 = lambda at: struct.unpack_from('<I', p, at)[0]
    assert p[:8] == b'dex\n035\0' and u32(32) == len(p) and u32(36) == 112
    assert hashlib.sha1(p[32:]).digest() == p[12:32]
    assert zlib.adler32(p[12:]) == u32(8)
    assert u32(40) == 0x12345678 and u32(104) == len(p) - u32(108)

    def leb(at):
        value, shift = 0, 0
        while True:
            byte = p[at]
            at += 1
            value |= (byte & 127) << shift
            shift += 7
            assert shift <= 35
            if not byte & 128:
                return value, at

    strings = []
    for i in range(u32(56)):
        length, at = leb(u32(u32(60) + 4 * i))
        end = p.index(0, at)
        text = p[at:end].decode('ascii')
        assert length == len(text)
        strings.append(text)
    assert strings == sorted(set(strings))
    types = [u32(u32(68) + 4 * i) for i in range(u32(64))]
    assert types == sorted(set(types))
    assert {strings[i] for i in types} == {'I', 'Ltest/Answer;', 'Ljava/lang/Object;'}
    proto = u32(76)
    assert strings[u32(proto)] == 'I' and strings[types[u32(proto + 4)]] == 'I'
    assert not u32(proto + 8)
    method = u32(92)
    assert strings[types[u16(method)]] == 'Ltest/Answer;'
    assert u16(method + 2) == 0 and strings[u32(method + 4)] == 'answer'
    cls = u32(100)
    assert u32(cls) == u16(method) and strings[types[u32(cls + 8)]] == 'Ljava/lang/Object;'
    at = u32(cls + 24)
    values = []
    for _ in range(7):
        value, at = leb(at)
        values.append(value)
    assert values[:6] == [0, 0, 1, 0, 0, 9]
    code = values[6]
    assert code % 4 == 0 and u16(code) == 1 and u16(code + 2) == 0 and u16(code + 6) == 0
    assert u32(code + 12) == 4 and p[code + 16:code + 24] == bytes.fromhex('14002a0000000f00')
    map_at = u32(52)
    records = [struct.unpack_from('<HHII', p, map_at + 4 + 12 * i) for i in range(u32(map_at))]
    assert all(unused == 0 and size > 0 for _, unused, size, _ in records)
    assert len({typ for typ, _, _, _ in records}) == len(records)
    assert [off for _, _, _, off in records] == sorted(off for _, _, _, off in records)
    assert (0x2001, 0, 1, code) in records and (0x1000, 0, 1, map_at) in records


def java_class(path):
    """Original Java 5 class fixture: public static int sum(int n)."""
    u2 = lambda v: struct.pack('>H', v)
    u4 = lambda v: struct.pack('>I', v)
    utf = lambda text: b'\1' + u2(len(text)) + text.encode('ascii')
    code = bytes.fromhex('033c033d1c1aa2000d1b1c603c840201a7fff41bac')
    pool = [utf('Test'), b'\7' + u2(1), utf('java/lang/Object'), b'\7' + u2(3),
            utf('sum'), utf('(I)I'), utf('Code')]
    body = u2(2) + u2(3) + u4(len(code)) + code + u2(0) + u2(0)
    p = u4(0xcafebabe) + u2(0) + u2(49) + u2(len(pool) + 1) + b''.join(pool)
    p += u2(0x21) + u2(2) + u2(4) + u2(0) + u2(0) + u2(1)
    p += u2(9) + u2(5) + u2(6) + u2(1) + u2(7) + u4(len(body)) + body + u2(0)
    path.write_bytes(p)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--abi', type=Path, required=True)
    parser.add_argument('--compiler', default='cc')
    parser.add_argument('--sanitizers', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='mulib-android-', dir=parent) as directory:
        work = Path(directory)
        dex = work / 'classes.dex'
        run([str(args.runtime.resolve()), '--dex', str(dex)])
        dex_check(dex)
        cls = work / 'Test.class'
        java_class(cls)
        run([str(args.runtime.resolve()), '--class', str(cls)])
        if shutil.which('javap'):
            run(['javap', '-c', str(cls)])
        source = work / 'aot.c'
        run([str(args.runtime.resolve()), '--aot', str(source)])
        driver = work / 'driver.c'
        driver.write_text('''#define MULIB_IMPL_MIR
#include "mulib-mir.h"
#include "aot.c"
#include <assert.h>
int main(void) {
  mulib_mir_frame_t f={0}; mulib_mir_value_t args[2];
  args[0].integer=INT32_MAX; args[1].integer=1;
  assert(mulib_mir_frame_init(&f,mulib_generated_android_program(),args,2,NULL,NULL,100000,NULL)==MULIB_MIR_OK);
  f.remaining=100000;
  assert(mulib_generated_android(&f)==MULIB_MIR_FINISHED && f.result.integer==INT32_MIN);
  mulib_mir_frame_free(&f); return 0;
}
''')
        compiler = shlex.split(args.compiler)
        flags = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I' + str(root / 'include')]
        if args.sanitizers:
            flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
        run(compiler + flags + [str(driver), '-lm', '-o', str(work / 'aot')])
        run([str(work / 'aot')])
        run([str(args.abi.resolve()), str(work)])
        readers = 0
        if shutil.which('readelf'):
            for path in sorted(work.glob('elf-*.bin')):
                run(['readelf', '-h', '-S', str(path)])
            readers += 1
        if shutil.which('llvm-readobj'):
            for prefix in ['coff', 'pe', 'macho']:
                for path in sorted(work.glob(prefix + '-*.bin')):
                    run(['llvm-readobj', '--file-headers', '--sections', str(path)])
            readers += 1
        print(f'Android: independent DEX hashes/layout, JVM class fixture, executable C AOT, {readers} external object readers OK')


if __name__ == '__main__':
    main()
