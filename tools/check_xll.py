#!/usr/bin/env python3
"""Compile XLL-emitted C, compare native results, and link without XLL runtime."""
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emitter', required=True)
    parser.add_argument('--compiler', default=os.environ.get('CC', 'cc'))
    parser.add_argument('--sanitizers', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    flags = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG']
    if args.sanitizers:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
    links = [] if os.name == 'nt' else ['-lm', '-pthread']
    if sys.platform.startswith(('linux', 'freebsd')):
        links += ['-ldl']
    cxx = shlex.split(os.environ.get('CXX', 'c++'))
    with tempfile.TemporaryDirectory(prefix='mulib-xll-', dir=parent) as work:
        work = Path(work)
        for mode, defines in [('--emit', []), ('--emit', ['MULIB_XLL_NO_REFERENCE']),
                              ('--emit-empty', ['MULIB_XLL_EMPTY', 'MULIB_XLL_NO_REFERENCE']),
                              ('--emit-long', ['MULIB_XLL_LONG', 'MULIB_XLL_NO_REFERENCE'])]:
            with (work / 'xll-generated.c').open('wb') as output:
                subprocess.run([args.emitter, mode], stdout=output, check=True, timeout=60)
            source = (work / 'xll-generated.c').read_text()
            assert '#include "mulib-layout.h"' in source
            assert '#include "mulib-xll.h"' not in source and 'mulib_xll_parse(' not in source
            binary = work / 'driver'
            command = shlex.split(args.compiler) + flags + ['-D' + d for d in defines]
            command += ['-I' + str(root / 'include'), '-I' + str(work),
                        str(root / 'tests/fixtures/xll_driver.c')] + links + ['-o', str(binary)]
            subprocess.run(command, check=True, timeout=120)
            subprocess.run([str(binary)], check=True, timeout=60)
            if cxx and shutil.which(cxx[0]):
                subprocess.run(cxx + ['-x', 'c++', '-std=c++17', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                                     '-I' + str(root / 'include'), '-c', str(work / 'xll-generated.c'),
                                     '-o', str(work / 'generated.o')], check=True, timeout=60)
    print('XLL generated C: metadata, native layout parity, standalone, empty and long text passed')


if __name__ == '__main__':
    main()
