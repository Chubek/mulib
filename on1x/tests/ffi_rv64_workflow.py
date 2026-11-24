#!/usr/bin/env python3
"""Checked-HIR FFI → RV64IM LP64 integration; requires a real clamp catalogue."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'on1x/tools'))
from on1x_ffi_rv64 import TARGET, check_elf


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('on1xc', 'cc', 'linker', 'runner', 'cephyr-prefix', 'work'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    args.work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='ffi-rv64-driver-', dir=args.work) as tmp:
        stage = Path(tmp)

        def run(argv, *, code=0, diagnostic=None):
            result = subprocess.run(argv, cwd=stage, capture_output=True, text=True, timeout=150)
            if ((code is None and result.returncode == 0) or
                    (code is not None and result.returncode != code) or
                    (diagnostic is not None and diagnostic not in result.stderr)):
                raise RuntimeError(f'{argv!r} returned {result.returncode}: {result.stderr}')
            return result.stdout

        def build(source, name, impl='host', adapter='host', *, extra=(), diagnostic=None):
            out = stage / name
            run([sys.executable, '-B', ROOT / 'on1x/tools/on1x-build.py', source, '-o', out,
                 '--on1xc', args.on1xc, '--cc', args.cc, '--mulib-root', ROOT, '--no-prelude',
                 '--ffi-provider', impl, '--ffi-adapter-provider', adapter,
                 '--ffi-cross-runner', args.runner, '--ffi-rv64-linker', args.linker,
                 '--ffi-rv64-toolchain', args.cephyr_prefix, *extra],
                code=None if diagnostic else 0, diagnostic=diagnostic)
            if diagnostic and out.exists():
                raise RuntimeError(f'failed build published {out}')
            return out

        sample = ROOT / 'on1x/examples/kernel/clamp.on1x'
        expected = '42\n9223372036854775807\n-9223372036854775808\n9223372036854775807\n-9223372036854775808\n6\n0\n10\n4\n-7\n'
        plan = build(sample, 'plan.json', extra=('--inspect-ffi',))
        report = json.loads(plan.read_bytes())
        if report['target'] != TARGET or report['admission'] != 'not_qualified':
            raise RuntimeError('discovery changed the target or granted admission')
        for impl in ('host', 'cephyr'):
            for adapter in ('host', 'cephyr'):
                program = build(sample, impl + '-' + adapter, impl, adapter)
                check_elf(program, 2)
                if run([args.runner, program]) != expected:
                    raise RuntimeError(f'wrong canonical clamp results: {impl}/{adapter}')

        bad = stage / 'bad.on1x'
        bad.write_text('unsafe import kernel mulib.clamp as C;\n'
                       'fn main() { unsafe { println(C.i64(10, 20, 19)); } }\n')
        trap = build(bad, 'trapping', 'cephyr', 'cephyr')
        run([args.runner, trap], code=70, diagnostic='On1x assertion failed')
        bad.write_text('unsafe import kernel mulib.clamp as C;\nfn main() { println(C.add_i64(40, 2)); }\n')
        build(bad, 'unscoped', diagnostic='requires unsafe')
        bad.write_text(sample.read_text().replace('import kernel', 'import kernel[dynlib]', 1))
        build(bad, 'dynamic', diagnostic='supports static imports only')
        safe = ROOT / 'on1x/examples/kernel/clamp_safe.on1x'
        build(safe, 'safe', extra=('--ffi-review-candidate',), diagnostic='safe imports are not qualified')
        build(sample, 'mir', extra=('--backend=mir-archon',), diagnostic='requires direct C11')

        staged = run([args.on1xc, '--ffi-imports', sample, '--no-prelude'])
        descriptor = json.loads(staged)['descriptors'][0]
        adapter = stage / 'generated.c'
        run([args.on1xc, '--ffi-adapter', '--ffi-id', descriptor['id'], sample, '--no-prelude', '-o', adapter])
        run([args.cc, '--target=riscv64-linux-gnu', '-march=rv64imafd', '-mabi=lp64d', '-nostdinc',
             '-isystem', args.cephyr_prefix / 'share/cephyr/stdlib/include',
             '-I', ROOT / 'on1x/runtime', '-I', ROOT / 'include', '-DON1X_KERNEL_ABI_V1=1',
             '-std=c11', '-fsyntax-only', adapter],
            code=None, diagnostic='FFIHandler requires the verified native Linux RV64IM LP64 build profile')
    print('RV64IM LP64 checked FFI: four provider pairs, guard trap, plan-only inspection, '
          'lexical unsafe enforcement and safe/DSO/MIR/LP64D refusal passed')


if __name__ == '__main__':
    main()
