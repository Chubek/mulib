#!/usr/bin/env python3
"""Compile, link and exercise the daemon byte dispatcher using real KernelBind."""
import argparse
import json
from pathlib import Path
import platform
import re
import subprocess
import sys
import tempfile


PACKAGE = Path(__file__).resolve().parent
ON1X = PACKAGE.parents[1]


def run(*command, diagnostic=None, preexec_fn=None):
    proc = subprocess.run(command, capture_output=True, text=True, timeout=90,
                          preexec_fn=preexec_fn)
    if diagnostic is None:
        assert proc.returncode == 0, proc
    else:
        assert proc.returncode != 0 and diagnostic in proc.stderr, proc
    return proc.stdout


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--on1xc', required=True, type=Path)
    cli.add_argument('--exotract', required=True, type=Path)
    cli.add_argument('--cc', default='cc')
    cli.add_argument('--clang', default='clang', help='Clang supplying the Exotract resource directory')
    args = cli.parse_args()
    if (sys.platform != 'linux' or platform.machine() != 'x86_64' or
            not re.fullmatch(r'x86_64(?:-[a-z0-9_]+)?-linux-gnu', run(args.cc, '-dumpmachine').strip())):
        print('UNSUPPORTED: daemon IPC fixture requires native Linux x86-64 GNU ABI')
        return 77

    import resource

    def limit_fds(soft):
        def apply_limit():
            _, hard = resource.getrlimit(resource.RLIMIT_NOFILE)
            resource.setrlimit(resource.RLIMIT_NOFILE, (soft, hard))
        return apply_limit

    frontend = (args.on1xc.resolve(strict=True), '--emit-c', '--no-prelude')
    driver = (sys.executable, ON1X/'tools/on1x-build.py', '--kernel-bind', '--no-prelude',
              '--on1xc', frontend[0], '--exotract', args.exotract.resolve(strict=True),
              '--cc', args.cc, '--resource-dir', run(args.clang, '-print-resource-dir').strip())
    with tempfile.TemporaryDirectory(prefix='on1x-daemon-ipc-') as tmp:
        work = Path(tmp)
        fixture = PACKAGE/'fixtures/dispatch.on1x'
        program, plan = work/'dispatch', work/'plan.json'
        run(*driver, fixture, '-o', program, '--mulib-plan', plan)
        assert run(program, preexec_fn=limit_fds(32)) == '0\n255\n260\n'

        link = json.loads(plan.read_text(encoding='utf-8'))
        interface = link['kernel_interface']
        assert interface['module'] == 'mulib.ipc_framed'
        assert interface['profile'] == 'unsafe-ipc-framed-local-pair/v1'
        assert link['safe_export'] is interface['safe_export'] is False
        assert link['plan']['implementation_translation_units'] == 1
        assert link['plan']['implementation_macros'] == [
            'MULIB_IMPL_SANDBOX_PROFILE', 'MULIB_IMPL_IPC_FRAMED']

        exhausted = work/'socket_exhausted'
        run(*driver, PACKAGE/'fixtures/socket_exhausted.on1x', '-o', exhausted)
        assert run(exhausted, preexec_fn=limit_fds(4)) == ''

        output = work/'not-emitted.c'
        run(*frontend, fixture, '-o', output, diagnostic='ON1X-KERNEL-UNSAFE-UNSUPPORTED')
        assert not output.exists()
        probe = work/'admission.on1x'
        for module, spellings in (('ipc_framed', ('',)), ('daemon', ('', 'unsafe ')),
                                  ('process', ('', 'unsafe '))):
            for prefix in spellings:
                probe.write_text(f'{prefix}import kernel mulib.{module};\nfn main() {{}}\n',
                                 encoding='ascii')
                diagnostic = ('ON1X-KERNEL-UNSAFE-UNSUPPORTED' if prefix else
                              'ON1X-KERNEL-NOT-ADMITTED')
                run(*frontend, '--kernel-c11', '--kernel-stage', probe, '-o', output,
                    diagnostic=diagnostic)
                assert not output.exists()
    print('daemon IPC: byte dispatch, bounds, descriptor failure/cleanup, and admission PASS')
    return 0


if __name__ == '__main__':
    sys.exit(main())
