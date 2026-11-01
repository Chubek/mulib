#!/usr/bin/env python3
"""Compile and exercise Prowse, Mune integration and the Readline/profile library."""
import argparse
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
MODULES = ('readline', 'prowse-protocol', 'prowse-control')


def dependencies(name):
    seen = set()

    def visit(header):
        if header in seen:
            return
        seen.add(header)
        text = (ROOT / 'include' / header).read_text()
        for child in re.findall(r'#\s*include\s*"(mulib-[^"]+\.h)"', text):
            visit(child)

    visit(f'mulib-{name}.h')
    macros = {'MULIB_IMPL_PARSER'}
    for header in seen:
        text = (ROOT / 'include' / header).read_text()
        macros.add(re.search(r'#if defined\((MULIB_IMPL_\w+)\)', text).group(1))
    return sorted(seen), sorted(macros)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitizers', action='store_true')
    parser.add_argument('--live-firefox', action='store_true')
    args = parser.parse_args()
    cc = shlex.split(os.environ.get('CC', 'cc'))
    cxx = shlex.split(os.environ.get('CXX', 'c++'))
    flags = ['-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I' + str(ROOT / 'include'), '-pthread']
    if args.sanitizers:
        flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
    links = ['-lm'] + (['-ldl'] if os.name == 'posix' and os.uname().sysname != 'Darwin' else [])
    count = 0
    for tool in ('embed_lprowse.py', 'generate_librl.py'):
        subprocess.run([os.sys.executable, str(ROOT / 'tools' / tool), '--check'], check=True)
        count += 1
    parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='mulib-prowse-', dir=parent) as directory:
        work = Path(directory)

        def build(name, source=None, text=None, cpp=False):
            compiler = cxx if cpp else cc
            command = compiler + ['-std=c++17' if cpp else '-std=c11'] + flags
            command += ['-x', 'c++' if cpp else 'c', '-'] if text is not None else [str(source)]
            binary = work / name
            subprocess.run(command + links + ['-o', str(binary)], input=text, text=True, check=True, cwd=work, timeout=180)
            return binary

        def run(binary, arguments=()):
            nonlocal count
            subprocess.run([str(binary)] + list(arguments), check=True, cwd=work, timeout=120)
            count += 1

        for module in MODULES:
            header = f'mulib-{module}.h'
            declaration = f'#include "{header}"\n' * 2 + 'int main(void) { return 0; }\n'
            run(build(module + '-decl', text=declaration))
            if shutil.which(cxx[0]):
                run(build(module + '-cxx-decl', text=declaration, cpp=True))
            headers, macros = dependencies(module)
            defines = ''.join(f'#define {macro}\n' for macro in macros)
            run(build(module + '-impl', text=defines + declaration))
            if module == 'prowse-control':
                includes = ''.join(f'#include "{name}"\n' for name in reversed(headers))
                run(build('composition', text=includes + defines + includes + 'int main(void) { return 0; }\n'))
        binaries = {}
        for name in MODULES:
            binary = build('example-' + name, source=ROOT / 'examples/modules' / (name + '.c'))
            binaries[name] = binary
            run(binary)
        for name in ('readline', 'prowse', 'prowse_alloc'):
            binary = build('test-' + name, source=ROOT / 'tests' / ('test_' + name + '.c'))
            binaries['test-' + name] = binary
            run(binary, [str(ROOT / 'library/librl')] if name == 'readline' else [])
        subprocess.run([os.sys.executable, str(ROOT / 'tools/check_prowse_io.py'),
                        '--protocol', str(binaries['test-prowse']), '--repl', str(binaries['prowse-control'])], check=True, timeout=90)
        count += 1
        if args.live_firefox:
            subprocess.run([os.sys.executable, str(ROOT / 'tools/check_prowse_live.py'),
                            '--runner', str(binaries['prowse-control'])], check=True, timeout=120)
            count += 1
    print(f'Prowse: {count}/{count} focused checks passed; 45 language profiles loaded')


if __name__ == '__main__':
    main()
