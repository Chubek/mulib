#!/usr/bin/env python3
"""Strict text/font/layout tests, examples and optional native ABI comparisons."""
import argparse
import os
import re
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
MODULES = ('sysfonts', 'bidi', 'unibreak', 'pglayout', 'distfield',
           'fontshape', 'fontdraw', 'stylesheet', 'uniproc', 'uniname')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitizers', action='store_true')
    parser.add_argument('--require-backends', action='store_true')
    args = parser.parse_args()
    cc = shlex.split(os.environ.get('CC', 'cc'))
    flags = ['-std=c11', '-O2', '-g', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-I' + str(ROOT / 'include')]
    if args.sanitizers:
        flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                  '-fno-omit-frame-pointer']
    links = ['-lm'] if os.name != 'nt' else []
    if os.sys.platform.startswith(('linux', 'freebsd')):
        links += ['-ldl']
    env = os.environ.copy()
    if args.require_backends:
        env['MULIB_TEXT_REQUIRE_BACKENDS'] = '1'
    temporary_parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='mulib-text-', dir=temporary_parent) as directory:
        work = Path(directory)

        def check(source, extra=()):
            binary = work / source.stem
            subprocess.run(cc + flags + [str(source)] + list(extra) + links +
                           ['-o', str(binary)], check=True, cwd=work, timeout=120)
            subprocess.run([str(binary)], check=True, cwd=work, env=env, timeout=120)
            print('PASS', source.relative_to(ROOT), flush=True)

        for name in ('unicode_text', 'font_stack', 'distfield', 'pglayout', 'text_allocations', 'text_edgecases'):
            check(ROOT / 'tests' / f'test_{name}.c')
        for name in MODULES:
            check(ROOT / 'examples/modules' / f'{name}.c')
        # Derive closures from actual includes: independently link each module
        # and check declarations-before-implementations in reverse include order.
        def closure(name, found):
            if name in found:
                return
            text = (ROOT / 'include' / f'mulib-{name}.h').read_text()
            found[name] = re.search(r'^#\s*if.*?(MULIB_IMPL_[A-Z0-9_]+)', text, re.M).group(1)
            for dependency in re.findall(r'^\s*#\s*include\s*"mulib-([^"]+)\.h"', text, re.M):
                closure(dependency, found)

        for name in MODULES:
            found = {}
            closure(name, found)
            source = work / f'isolated-{name}.c'
            source.write_text(''.join(f'#define {switch}\n' for switch in found.values()) +
                              f'#include "mulib-{name}.h"\nint main(void) {{return 0;}}\n')
            subprocess.run(cc + flags + [str(source)] + links + ['-o', str(work / 'isolated')], check=True, timeout=120)
        found = {}
        for name in MODULES:
            closure(name, found)
        declarations = ''.join(f'#include "mulib-{name}.h"\n' for name in found)
        source = work / 'implementation.c'
        source.write_text(declarations + ''.join(f'#define {switch}\n' for switch in found.values()) +
                          ''.join(f'#include "mulib-{name}.h"\n' for name in reversed(found)))
        client = work / 'client.c'
        client.write_text(declarations + 'int main(void) {return mulib_stylesheet_default().font_size != 16;}\n')
        subprocess.run(cc + flags + [str(source), str(client)] + links + ['-o', str(work / 'composition')], check=True, timeout=120)
        subprocess.run([str(work / 'composition')], check=True, env=env, timeout=120)
        print('PASS isolated modules and separate-TU reverse-order implementations', flush=True)
        cxx = shlex.split(os.environ.get('CXX', 'c++'))
        if cxx and shutil.which(cxx[0]):
            source = work / 'declarations.cpp'
            source.write_text(''.join(f'#include "mulib-{m}.h"\n' for m in MODULES) +
                              'int main() { return 0; }\n')
            subprocess.run(cxx + ['-std=c++17'] + flags[1:] + [str(source)] + links +
                           ['-o', str(work / 'declarations')], check=True, timeout=120)
            print('PASS C++ declarations', flush=True)
        packages = ['freetype2', 'harfbuzz', 'icu-uc', 'icu-i18n']
        if shutil.which('pkg-config') and subprocess.run(
                ['pkg-config', '--exists'] + packages, check=False).returncode == 0:
            extra = shlex.split(subprocess.check_output(
                ['pkg-config', '--cflags', '--libs'] + packages, text=True))
            check(ROOT / 'tests/native/text_backends.c', extra)
        else:
            print('SKIP native reference comparison: development packages unavailable')


if __name__ == '__main__':
    main()
