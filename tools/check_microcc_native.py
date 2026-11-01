#!/usr/bin/env python3
"""Assemble MicroCC output, run native ABI/semantic checks, and cross-assemble RV64IM."""
import argparse
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emitter', required=True)
    parser.add_argument('--compiler', default=os.environ.get('CC', 'cc'))
    parser.add_argument('--sanitizers', action='store_true')
    args = parser.parse_args()
    emitter = str(Path(args.emitter).resolve())
    have_rv64 = False
    if shutil.which('clang'):
        targets = subprocess.run(['clang', '--print-targets'], capture_output=True,
                                 text=True, timeout=15)
        have_rv64 = targets.returncode == 0 and 'riscv64' in targets.stdout
    parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='mulib-microcc-', dir=parent) as directory:
        work = Path(directory)
        for target in ('x86', 'rv64'):
            assembly = work / (target + '.s')
            with assembly.open('w') as output:
                subprocess.run([emitter, '--emit', target], stdout=output, check=True, timeout=60)
            if target == 'x86' and platform.system() == 'Linux' and platform.machine().lower() in ('x86_64', 'amd64'):
                executable = work / 'native'
                flags = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG', '-Wa,--fatal-warnings']
                if args.sanitizers:
                    flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
                subprocess.run(shlex.split(args.compiler) + flags + [str(assembly),
                    str(ROOT / 'tests/microcc_native_driver.c'), '-o', str(executable)], check=True, timeout=60)
                subprocess.run([str(executable)], check=True, timeout=60)
                print('MicroCC x86-64 assembly: linked and executed arithmetic, PHIs, memory, calls and ABI checks')
            elif target == 'rv64' and have_rv64:
                subprocess.run(['clang', '--target=riscv64', '-march=rv64im', '-mabi=lp64',
                    '-c', str(assembly), '-o', str(work / 'rv64.o')], check=True, timeout=60)
                print('MicroCC RV64IM assembly: cross-assembled with Clang')
            else:
                print('SKIP: assembler/execution host unavailable for ' + target)


if __name__ == '__main__':
    main()
