#!/usr/bin/env python3
"""Independent strict VMM/Virtio checks, including separate-TU linking and C++ declarations."""
import argparse
import os
import re
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
MODULES = ['endian', 'vmmdev', 'virtio', 'virtio-os', 'virtio-guest',
           'vmm-memory', 'vmm', 'vmm-host', 'vmm-kvm']


def dependencies(name):
    ordered = []

    def visit(module):
        if module in ordered:
            return
        text = (ROOT / 'include' / f'mulib-{module}.h').read_text()
        for dependency in re.findall(r'^#include "mulib-([^"\n]+)\.h"', text, re.M):
            visit(dependency)
        ordered.append(module)

    visit(name)
    return ordered


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitizers', action='store_true')
    args = parser.parse_args()
    compilers = [shlex.split(os.environ['CC'])] if os.environ.get('CC') else [
        [cc] for cc in ('gcc', 'clang') if shutil.which(cc)]
    if not compilers:
        parser.error('no C compiler available; set CC')
    flags = ['-std=c11', '-O1' if args.sanitizers else '-O2', '-Wall', '-Wextra',
             '-Wpedantic', '-Werror', '-UNDEBUG', '-I' + str(ROOT / 'include')]
    if args.sanitizers:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                  '-fno-omit-frame-pointer']
    with tempfile.TemporaryDirectory(prefix='mulib-virtio-', dir='/tmp/opencode') as tmp:
        work = Path(tmp)
        serial = 0

        def build(cc, sources=None, text=None, extra=None, run=True, cxx=False):
            nonlocal serial
            serial += 1
            output = work / ('case-' + str(serial))
            local_flags = [f for f in flags if f != '-std=c11'] if cxx else flags
            command = cc + local_flags + (['-std=c++17'] if cxx else []) + (extra or [])
            if text is not None:
                command += ['-x', 'c++' if cxx else 'c', '-']
            else:
                command += [str(ROOT / p) for p in sources]
            command += ['-o', str(output)]
            subprocess.run(command, input=text, text=True, check=True, cwd=work)
            if run:
                result = subprocess.run([str(output)], check=True, cwd=work, stdout=subprocess.PIPE, text=True)
                if sources and 'tests/test_vmm_kvm.c' in sources:
                    print(result.stdout.strip(), flush=True)

        for cc in compilers:
            for source in ['tests/test_vmmdev.c', 'tests/test_virtio.c', 'tests/test_vmm.c', 'tests/test_vmm_kvm.c'] + [
                    f'examples/modules/{name}.c' for name in MODULES[1:]]:
                build(cc, [source])
            for name in MODULES[1:]:
                deps = dependencies(name)
                macros = ''.join('#define MULIB_IMPL_' + d.upper().replace('-', '_') + '\n' for d in deps)
                header = f'#include "mulib-{name}.h"\n'
                implementations = ''.join(f'#include "mulib-{d}.h"\n' for d in deps)
                build(cc, text=header + macros + implementations + 'int main(void) { return 0; }\n')
            # Public contract alias also supports implementation after declarations.
            build(cc, text='#include "virtio.h"\n#define MULIB_IMPL_ENDIAN\n'
                  '#define MULIB_IMPL_VMMDEV\n#define MULIB_IMPL_VIRTIO\n'
                  '#include "mulib-endian.h"\n#include "mulib-vmmdev.h"\n#include "virtio.h"\n'
                  'int main(void) { return mulib_virtio_profile_count() == 48 ? 0 : 1; }\n')
            build(cc, ['library/virtio-os/protocol.c', 'library/virtio-os/virtio-os.c',
                       'library/virtio-guest/virtio-guest.c', 'examples/modules/virtio-guest.c'],
                  extra=['-DMULIB_VIRTIO_EXAMPLE_COMPILED'])
            print('Virtio C checks passed:', ' '.join(cc), flush=True)
        cxx_compilers = [shlex.split(os.environ['CXX'])] if os.environ.get('CXX') else [
            [cc] for cc in ('g++', 'clang++') if shutil.which(cc)]
        for cc in cxx_compilers:
            for name in MODULES[1:]:
                build(cc, text=f'#include "mulib-{name}.h"\nint main() {{ return 0; }}\n', cxx=True)
            print('Virtio C++ declarations passed:', ' '.join(cc), flush=True)
    print('VMM/Virtio focused checks passed' + (' with ASan/UBSan' if args.sanitizers else ''))


if __name__ == '__main__':
    main()
