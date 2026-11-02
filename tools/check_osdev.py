#!/usr/bin/env python3
"""Strict hosted and freestanding checks for the allocation-free OSDev headers.

CC/CXX select compilers. --sanitizers instruments hosted examples/tests only.
Privileged code is compiled, never executed. No hardware tests are performed.
"""
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
MODULES = ('efi', 'bootmgr', 'hwinfo', 'memtest', 'gputest', 'descritors',
           'io', 'paging', 'pci', 'acpi', 'framebuffer')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitizers', action='store_true')
    args = parser.parse_args()
    cc = shlex.split(os.environ.get('CC', 'cc'))
    cxx = shlex.split(os.environ.get('CXX', 'c++'))
    flags = ['-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-I' + str(ROOT / 'include')]
    host_flags = (['-g', '-fsanitize=address,undefined',
                   '-fno-sanitize-recover=all'] if args.sanitizers else [])
    bare_flags = ['-ffreestanding', '-fno-builtin', '-fno-stack-protector']
    declarations = ''.join('#include "mulib-' + m + '.h"\n' for m in MODULES)
    switches = ''.join('#define MULIB_IMPL_' + m.upper() + '\n' for m in MODULES)

    def run(command):
        subprocess.run(command, check=True)

    with tempfile.TemporaryDirectory(prefix='mulib-osdev-', dir='/tmp/opencode') as tmp:
        tmp = Path(tmp)
        for module in MODULES:
            unit = tmp / (module + '.c')
            unit.write_text('#define MULIB_IMPL_' + module.upper() + '\n'
                            '#include "mulib-' + module + '.h"\n')
            run(cc + ['-std=c11'] + flags + bare_flags + [
                '-c', str(unit), '-o', str(tmp / (module + '.o'))])
            binary = tmp / module
            run(cc + ['-std=c11'] + flags + host_flags +
                [str(ROOT / 'examples/modules' / (module + '.c')), '-o', str(binary)])
            run([str(binary)])
        implementation = tmp / 'implementation.c'
        implementation.write_text(switches + declarations)
        run(cc + ['-std=c11'] + flags + bare_flags + ['-c',
            str(implementation), '-o', str(tmp / 'implementation.o')])
        bare_client = tmp / 'bare-client.c'
        bare_client.write_text(declarations + 'int main(void) { return 0; }\n')
        # Link-only check: no CRT or libc, and never execute this non-hosted image.
        run(cc + ['-std=c11'] + flags + bare_flags + ['-nostdlib', '-no-pie',
            '-Wl,-e,main', str(bare_client), str(tmp / 'implementation.o'),
            '-o', str(tmp / 'bare-image')])
        client = tmp / 'client.cpp'
        client.write_text(declarations + '#include "mulib-efi"\n'
                          'int main() { return mulib_paging_canonical(0, 48) ? 0 : 1; }\n')
        run(cxx + ['-std=c++17'] + flags + [str(client), str(tmp / 'implementation.o'),
            '-o', str(tmp / 'client')])
        run([str(tmp / 'client')])
        run(cc + ['-std=c11'] + flags + bare_flags + [
            '-DMULIB_IO_ENABLE_PRIVILEGED', '-c', str(tmp / 'io.c'),
            '-o', str(tmp / 'io-privileged.o')])
        run(cc + ['-std=c11'] + flags + host_flags + [str(ROOT / 'tests/test_osdev.c'),
            '-o', str(tmp / 'regressions')])
        run([str(tmp / 'regressions')])
        run(cc + ['-std=c11'] + flags + host_flags + [str(ROOT / 'tests/test_osdev_loaders.c'),
            '-o', str(tmp / 'loaders')])
        run([str(tmp / 'loaders')])
        validator = shutil.which('glslangValidator')
        if validator:
            emitter = tmp / 'shader.c'
            emitter.write_text('#define MULIB_IMPL_GPUTEST\n#include "mulib-gputest.h"\n'
                               '#include <stdio.h>\nint main(void) { '
                               'return fputs(mulib_gputest_shader(), stdout) < 0; }\n')
            run(cc + ['-std=c11'] + flags + [str(emitter), '-o', str(tmp / 'shader')])
            source = tmp / 'stress.comp'
            source.write_bytes(subprocess.check_output([str(tmp / 'shader')]))
            run([validator, '-S', 'comp', str(source)])
            print('GPU workload: GLSL compute shader validation passed')
    print('OSDev: 11 freestanding headers/examples, C++ linkage and hosted regressions passed')


if __name__ == '__main__':
    main()
