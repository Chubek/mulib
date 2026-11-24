#!/usr/bin/env python3
"""Verify the real RV64 static link path; never grants On1x FFI admission."""

import argparse
from pathlib import Path
import subprocess
import tempfile

ON1X = Path(__file__).resolve().parents[1]
from rv64_abi_toolchain import PREPROCESSOR_ABI, Toolchain, check_elf


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ('cc', 'linker', 'runner', 'cephyr', 'headers', 'runtime', 'work'):
        parser.add_argument('--' + arg, required=True, type=Path)
    args = parser.parse_args()
    fixture = ON1X / 'tests/fixtures/ffi_rv64'
    include = ON1X.parent / 'kernels'

    args.work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='ffi-rv64-', dir=args.work) as tmp:
        stage = Path(tmp).resolve()

        def command(argv, *, fail=False):
            result = subprocess.run(argv, cwd=stage, stdin=subprocess.DEVNULL,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
            if bool(result.returncode) != fail:
                raise RuntimeError(f'{argv!r} returned {result.returncode}: {result.stderr.decode()}')
            return result

        def run(argv):
            return command(argv).stdout

        tool = Toolchain(args.cc, args.linker, args.runner, args.headers, args.runtime,
                         run=run, pin=lambda p: None)
        (stage / 'Cephyr.toml').write_text('')
        cephyr = [Toolchain.absolute(args.cephyr), '--config', stage / 'Cephyr.toml',
                  '--target=riscv64-linux-gnu', '--stdlib=cephyr', '--no-factir', '--no-sandslab']
        witness = stage / 'driver.c'
        witness.write_text(f'''#if !({PREPROCESSOR_ABI})
#error wrong RV64 calling convention
#endif
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <mulib-clamp.h>
#include "on1x_runtime.h"
_Static_assert(CHAR_BIT == 8 && CHAR_MIN == 0 && sizeof(long) == 8
               && sizeof(void *) == 8 && _Alignof(void *) == 8, "RV64 C ABI layout");
double clang_caller(void);
double cephyr_caller(void);
int main(void) {{
    uint64_t clang_bits, cephyr_bits;
    double from_clang = clang_caller(), from_cephyr = cephyr_caller();
    memcpy(&clang_bits, &from_clang, sizeof(clang_bits));
    memcpy(&cephyr_bits, &from_cephyr, sizeof(cephyr_bits));
    if (clang_bits != UINT64_C(0xc004000000000000)) return 1;
    if (cephyr_bits != UINT64_C(0xc004000000000000)) return 2;
    if (mulib_clamp_add_i64(INT64_MAX, 1) != INT64_MAX) return 3;
    if (mulib_clamp_sub_i64(INT64_MIN, 1) != INT64_MIN) return 4;
    on1x_print_i64(mulib_clamp_i64(43, 0, 42));
    return 0;
}}
''')
        clamp = stage / 'clamp.c'
        clamp.write_text('#define MULIB_IMPL_CLAMP\n#include <mulib-clamp.h>\n')
        strict = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror']
        runtime = ON1X / 'runtime'
        objects = [stage / (name + '.o') for name in ('driver', 'clang', 'cephyr', 'oracle', 'clamp', 'on1x')]
        driver, clang, cc_cephyr, oracle, impl, rt = objects
        tool.driver(*strict, '-I', include, '-I', runtime, '-c', witness, '-o', driver)
        tool.driver(*strict, '-DABI_CALLER=clang_caller', '-c', fixture / 'mixed_call.c', '-o', clang)
        run([*cephyr, '-DABI_CALLER=cephyr_caller', '-c', fixture / 'mixed_call.c', '-o', cc_cephyr])
        tool.driver('-c', fixture / 'mixed_oracle.S', '-o', oracle)
        run([*cephyr, '-I', include, '-c', clamp, '-o', impl])
        tool.driver(*strict, '-c', runtime / 'on1x_runtime.c', '-o', rt)
        program = stage / 'probe'
        tool.link(program, objects)
        if tool.execute(program) != b'42\n':
            raise RuntimeError('wrong canonical clamp / On1x runtime output')

        # The same Linux triple can use a different ABI. Test a true
        # Clang-produced LP64D ELF and LLD's real mixed-object rejection.
        hard = stage / 'lp64d.o'
        command([tool.cc, '--target=riscv64-linux-gnu', '-march=rv64imafd', '-mabi=lp64d',
                 '-c', fixture / 'mixed_oracle.S', '-o', hard])
        try:
            check_elf(hard, 1)
        except ValueError as error:
            if 'soft-float LP64' not in str(error):
                raise
        else:
            raise RuntimeError('incorrectly admitted an actual LP64D object')
        diagnostic = command([tool.linker, '-r', '-o', stage / 'mixed.o', clang, hard], fail=True)
        if not any(text in diagnostic.stderr for text in (b'floating-point ABI', b'floating point ABI')):
            raise RuntimeError(f'wrong mixed-link diagnostic: {diagnostic.stderr!r}')

        # Confirm that section GC cannot hide an unresolved independent
        # prototype relocation when no source call references its witness.
        missing = stage / 'missing.c'
        missing.write_text('extern void unavailable(void);\nvoid (*witness)(void) = unavailable;\n')
        missing_obj = stage / 'missing.o'
        tool.driver(*strict, '-c', missing, '-o', missing_obj)
        try:
            tool.link(stage / 'must-fail', [*objects, missing_obj], roots=('witness',))
        except RuntimeError as error:
            if 'undefined symbol: unavailable' not in str(error):
                raise
        else:
            raise RuntimeError('section GC discarded the rooted prototype relocation')
    print('RV64IM LP64: Clang and Cephyr register/stack ABI, canonical clamp/runtime, '
          'LP64D rejection, and retained witness passed')


if __name__ == '__main__':
    main()
