#!/usr/bin/env python3
"""Exercise the checked-C11 FFI driver with a real AAPCS64 catalogue and tools."""

import argparse
import json
from pathlib import Path
import resource
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'on1x/tools'))
import on1x_ffi_aarch64 as aarch64


def main():
    options = argparse.ArgumentParser(description=__doc__)
    options.add_argument('--on1xc', type=Path, required=True)
    options.add_argument('--cc', type=Path, required=True, help='GNU cross driver with static AArch64 libc')
    options.add_argument('--qemu', type=Path, required=True)
    options.add_argument('--host-cc', type=Path, required=True, help='AMD64 compiler for a negative control')
    options.add_argument('--embedded-cephyr', action='store_true')
    options.add_argument('--work', type=Path, required=True)
    args = options.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    with tempfile.TemporaryDirectory(prefix='aapcs64-', dir=args.work.resolve(strict=True)) as temp:
        work = Path(temp)

        def run(command, *, fails=False, message=b'', stdin=None):
            result = subprocess.run(command, input=stdin, capture_output=True, cwd=work, timeout=180)
            if (result.returncode != 0) != fails or message not in result.stderr:
                raise RuntimeError(f'{command}: exit {result.returncode}\n'
                                   + result.stderr.decode(errors='replace'))
            return result.stdout

        tool = args.on1xc.resolve(strict=True)
        cc = args.cc.resolve(strict=True)
        qemu = args.qemu.resolve(strict=True)
        host = args.host_cc.resolve(strict=True)
        driver = [sys.executable, '-B', ROOT / 'on1x/tools/on1x-build.py', '--on1xc', tool,
                  '--mulib-root', ROOT, '--no-prelude']
        cc_flags = ['--cc', cc, '--ffi-cross-runner', qemu]
        abi = ['-I', ROOT / 'include', '-I', ROOT / 'on1x/runtime', '-DON1X_KERNEL_ABI_V1=1']

        def source(name, body, *, safe=False, dynamic=False):
            path = work / f'{name}.on1x'
            path.write_text(('' if safe else 'unsafe ') + 'import kernel' +
                            ('[dynlib]' if dynamic else '') + ' mulib.clamp as C;\n'
                            'fn tap(x:i64)->i64 { println(x); return x; }\n'
                            f'fn main() {{ {body} }}\n', encoding='utf-8')
            return path

        valid = source('good', 'unsafe { println(C.i64(tap(10), tap(20), tap(30))); '
                       'println(C.add_i64(9223372036854775807, 1)); '
                       'println(C.sub_i64(-9223372036854775808, 1)); }')
        invalid_guard = source('bad-guard', 'unsafe { println(C.i64(0, 1, 0)); }')
        safe = source('safe', 'println(C.add_i64(1, 2));', safe=True)
        dyn = source('dyn', 'unsafe { println(C.add_i64(1, 2)); }', dynamic=True)
        no_unsafe = source('not-unsafe', 'println(C.i64(0, 0, 1));')
        expected = b'10\n20\n30\n20\n9223372036854775807\n-9223372036854775808\n'
        providers = ['--ffi-provider', 'host']

        def build(name, src, *, selection=providers, flags=cc_flags, extra=(), diagnostic=None):
            output = work / name
            run([*driver, *flags, *selection, *extra, src, '-o', output],
                fails=diagnostic is not None, message=diagnostic or b'')
            if output.exists() != (diagnostic is None):
                raise RuntimeError(f'{name}: wrong publication state')
            return output

        inspected = build('inspect.json', valid, extra=['--inspect-ffi'])
        plan = json.loads(inspected.read_bytes())
        if (plan['target'] != aarch64.TARGET or plan['safe_export'] or
                plan['admission'] != 'not_qualified'):
            raise RuntimeError('inspection did not remain target-bound and non-admitting')
        desc, = [row for row in plan['descriptors'] if row['member'] == 'i64']
        if desc['guards'] != [[1, 2]]:
            raise RuntimeError('AAPCS64 catalogue did not select the guarded clamp descriptor')

        # Compile the compiler-emitted adapter independently. A matching
        # LP64 width alone cannot let the AMD64 compiler build this adapter.
        adapter = work / 'adapter.c'
        run([tool, '--ffi-adapter', '--ffi-id', desc['id'], '--no-prelude', valid, '-o', adapter])
        host_obj = work / 'wrong-machine.o'
        run([host, *abi, '-c', adapter, '-o', host_obj], fails=True,
            message=b'verified native Linux AArch64 build profile')
        if host_obj.exists():
            raise RuntimeError('published AMD64 adapter for an AAPCS64 descriptor')
        obj = work / 'gnu-adapter.o'
        run([cc, *abi, '-c', adapter, '-o', obj])
        aarch64.check_elf(obj, (1,))
        if args.embedded_cephyr:
            helper = tool.with_name('on1x-cephyr-kernel')
            cephyr_object = work / 'cephyr-adapter.o'
            cephyr_object.write_bytes(run([helper, '--object', ROOT / 'include', ROOT / 'on1x/runtime',
                                           aarch64.TARGET[0]], stdin=adapter.read_bytes()))
            aarch64.check_elf(cephyr_object, (1,))

        modes = [('gnu-gnu', 'host', 'host')]
        if args.embedded_cephyr:
            modes += [('gnu-cephyr', 'host', 'cephyr'), ('cephyr-gnu', 'cephyr', 'host'),
                      ('cephyr-cephyr', 'cephyr', 'cephyr')]
        for name, implementation, bridge in modes:
            output = build(name, valid, selection=['--ffi-provider', implementation,
                                                   '--ffi-adapter-provider', bridge])
            aarch64.check_elf(output, (2,), static=True)
            if run([qemu, output]) != expected:
                raise RuntimeError(f'{name}: AAPCS64 executable oracle mismatch')

        trap = build('guard-traps', invalid_guard)
        run([qemu, trap], fails=True, message=b'assert')
        build('unsafe-context-required', no_unsafe, diagnostic=b'ON1X-KERNEL-CONTEXT')
        build('safe-build', safe, diagnostic=b'ON1X-FFI-NOT-ADMITTED')
        build('safe-candidate', safe, extra=['--ffi-review-candidate'], diagnostic=b'ON1X-FFI-NOT-ADMITTED')
        build('dso-not-qualified', dyn, diagnostic=b'static imports only')
        build('wrong-cc', valid, flags=['--cc', host, '--ffi-cross-runner', qemu],
              diagnostic=b'matching Linux GNU LP64 toolchain')
        build('no-cross-runner', valid, flags=['--cc', cc], diagnostic=b'matching Linux GNU LP64 toolchain')
        build('no-mir-bypass', valid, extra=['--backend', 'mir-archon'], diagnostic=b'requires direct C11')
        print(f'AArch64 checked-C11: {len(modes)} real provider combinations and refusal controls passed')


if __name__ == '__main__':
    main()
