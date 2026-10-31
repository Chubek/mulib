#!/usr/bin/env python3
"""Compile and execute the C emitted by a MIR regression executable."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emitter', required=True)
    parser.add_argument('--compiler', default=os.environ.get('CC', 'cc'))
    parser.add_argument('--include', default=str(Path(__file__).resolve().parent.parent / 'include'))
    parser.add_argument('--sanitizers', action='store_true')
    args = parser.parse_args()
    parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='mulib-mir-aot-', dir=parent) as work:
        source = Path(work) / 'generated.c'
        binary = Path(work) / 'generated'
        with source.open('w') as output:
            subprocess.run([args.emitter, '--emit'], stdout=output, check=True, timeout=60)
        flags = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG']
        if args.sanitizers:
            flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                      '-fno-omit-frame-pointer']
        subprocess.run(shlex.split(args.compiler) + flags + ['-I' + args.include,
                       str(source), '-lm', '-o', str(binary)], check=True, timeout=90)
        subprocess.run([str(binary)], check=True, timeout=60)


if __name__ == '__main__':
    main()
