#!/usr/bin/env python3
"""Execute OptML AMD64 output and independently cross-assemble RV64 output."""
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
    parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='mulib-isel-', dir=parent) as directory:
        work = Path(directory)
        amd64, rv64 = work / 'amd64.s', work / 'rv64.s'
        subprocess.run([emitter, '--emit', str(amd64)], check=True, timeout=60)
        subprocess.run([emitter, '--emit', str(rv64), 'rv64'], check=True, timeout=60)
        if platform.system() == 'Linux' and platform.machine().lower() in ('x86_64', 'amd64'):
            flags = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG', '-Wa,--fatal-warnings']
            if args.sanitizers:
                flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
            executable = work / 'native'
            subprocess.run(shlex.split(args.compiler) + flags + [str(amd64),
                           str(ROOT / 'tests/isel_native_driver.c'), '-o', str(executable)], check=True, timeout=60)
            subprocess.run([str(executable)], check=True, timeout=60)
            print('OptML AMD64: executed 20000 arithmetic, macro, memory and call cases')
        if shutil.which('clang'):
            targets = subprocess.run(['clang', '--print-targets'], capture_output=True, text=True, timeout=15)
            if targets.returncode == 0 and 'riscv64' in targets.stdout:
                subprocess.run(['clang', '--target=riscv64', '-march=rv64im', '-mabi=lp64',
                                '-c', str(rv64), '-o', str(work / 'rv64.o')], check=True, timeout=60)
                print('OptML RV64IM: cross-assembled with Clang')


if __name__ == '__main__':
    main()
