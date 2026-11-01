#!/usr/bin/env python3
"""Strict independent validation of the nine graphics modules and their dependencies."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
MODULES = 'cgeom collision glx immgtk linalg raytracing shaderx simdmath vectgfx'.split()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitizers', action='store_true')
    args = parser.parse_args()
    cc = shlex.split(os.environ.get('CC', 'cc'))
    cxx = shlex.split(os.environ.get('CXX', 'c++'))
    flags = ['-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG',
             '-I' + str(ROOT / 'include'), '-pthread']
    if args.sanitizers:
        flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                  '-fno-omit-frame-pointer']
    headers = {}

    def visit(name):
        if name in headers:
            return
        text = (ROOT / 'include' / name).read_text()
        headers[name] = text
        for dependency in re.findall(r'^\s*#\s*include\s*"(mulib-[^"]+\.h)"', text, re.M):
            visit(dependency)

    for module in MODULES:
        visit('mulib-' + module + '.h')
    switches = set()
    for text in headers.values():
        switches.update(re.findall(r'^#\s*if(?:def\s+|\s+defined\s*\(\s*)(MULIB_IMPL_[A-Z0-9_]+)', text, re.M))
    defines = ''.join('#define ' + s + '\n' for s in sorted(switches))
    includes = ''.join('#include "' + h + '"\n' for h in sorted(headers, reverse=True))
    sources = sorted((ROOT / 'tests').glob('test_graphics_*.c'))
    sources += [ROOT / 'tests/test_immgtk.c', ROOT / 'tests/test_simdmath_scalar.c']
    sources += [ROOT / 'examples/modules' / (m + '.c') for m in MODULES]
    parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='mulib-graphics-', dir=parent) as directory:
        work = Path(directory)

        def check(source):
            case = work / source.stem
            case.mkdir()
            binary = case / 'run'
            subprocess.run(cc + ['-std=c11'] + flags + [str(source), '-lm', '-ldl', '-o', str(binary)],
                           check=True, capture_output=True, text=True, timeout=180)
            subprocess.run([str(binary)], cwd=case, check=True, capture_output=True, text=True, timeout=60)
            return binary

        with ThreadPoolExecutor(max_workers=4) as pool:
            binaries = list(pool.map(check, sources))
        combined = work / 'combined'
        subprocess.run(cc + ['-std=c11'] + flags + ['-x', 'c', '-', '-lm', '-ldl', '-o', str(combined)],
                       input=includes + defines + includes + 'int main(void) { return 0; }\n',
                       text=True, check=True, timeout=180)
        subprocess.run([str(combined)], check=True, timeout=60)
        if shutil.which(cxx[0]):
            subprocess.run(cxx + ['-std=c++17', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                                 '-I' + str(ROOT / 'include'), '-x', 'c++', '-fsyntax-only', '-'],
                           input=includes, text=True, check=True, timeout=60)
        validator = shutil.which('glslangValidator')
        if validator:
            emitter = binaries[sources.index(ROOT / 'tests/test_graphics_languages.c')]
            subprocess.run([os.sys.executable, str(ROOT / 'tools/check_graphics_shaders.py'),
                            '--emitter', str(emitter), '--validator', validator], check=True)
    print(f'Graphics: {len(sources)} tests/examples, combined implementations and declarations passed')


if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as error:
        if error.stderr:
            print(error.stderr, file=os.sys.stderr)
        raise SystemExit(error.returncode)
