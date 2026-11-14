#!/usr/bin/env python3
"""Build one On1x file through the checked C11 backend (no shell interpolation).

Paths assume this directory is nested at <mulib>/on1x.
"""
import argparse
from pathlib import Path
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MULIB = ROOT.parent

def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument('input', type=Path)
    cli.add_argument('-o', '--output', type=Path, required=True)
    cli.add_argument('--on1xc', default=str(ROOT / 'build' / 'on1xc'))
    cli.add_argument('--cc', default=os.environ.get('CC', 'cc'))
    cli.add_argument('--emit-only', action='store_true')
    cli.add_argument('--extra-c', action='append', default=[], help='extra C translation unit to compile/link')
    cli.add_argument('--cflag', action='append', default=[], help='explicit extra C compiler option')
    cli.add_argument('--ppx', nargs=2, action='append', default=[], metavar=('LABEL', 'PROGRAM'))
    cli.add_argument('--ppx-schema', nargs=2, action='append', default=[], metavar=('LABEL', 'FILE'))
    cli.add_argument('--allow-source-ppx', action='store_true')
    cli.add_argument('--ppx-timeout-ms', type=int, default=5000)
    args = cli.parse_args()
    with tempfile.TemporaryDirectory(prefix='on1x-build-') as temp:
        generated = Path(temp) / 'main.c'
        compiler = [args.on1xc, '--emit-c', str(args.input), '-o', str(generated),
                    '--ppx-timeout-ms', str(args.ppx_timeout_ms)]
        if args.allow_source_ppx:
            compiler.append('--allow-source-ppx')
        for label, program in args.ppx:
            compiler += ['--ppx', label, program]
        for label, schema in args.ppx_schema:
            compiler += ['--ppx-schema', label, schema]
        subprocess.run(compiler, check=True)
        code = generated.read_text()
        if args.emit_only:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(code)
            return
        args.output.parent.mkdir(parents=True, exist_ok=True)
        cmd = [args.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
               '-I', str(ROOT / 'runtime'), '-I', str(MULIB / 'include'),
               str(generated), str(ROOT / 'runtime/on1x_runtime.c')]
        if 'on1x_json_' in code:
            header = MULIB / 'include/mulib-json.h'
            if not header.is_file():
                raise SystemExit('JSON program requires a canonical MuLIB checkout with include/mulib-json.h')
            cmd += [str(ROOT / 'runtime/on1x_json.c'), str(ROOT / 'runtime/on1x_mulib_impl.c'), '-lm']
        cmd += args.extra_c + args.cflag + ['-o', str(args.output.resolve())]
        subprocess.run(cmd, check=True)

if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)
