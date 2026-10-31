#!/usr/bin/env python3
"""Check the container stack independently of retired-header inventory changes.

Strict standalone C/C++ declarations, isolated implementations, include-order
composition, runnable examples, functional/failure-injection tests and optional
external ELF/Zstandard interoperability. CC/CXX select compilers.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
MODULES = """container rootfs imgbuilder containerboost container-kvmboost
container-oixboost container-qvmboost container-ipc container-network container-vfs
fuse image2container containerd container-runtime container-jitboost container-netdev
container-log container-packelf container-packzstd container-stub imgbuildspec
containterpool containerd-ctrlbridge conainerd-ipc containerd-cache rootfs-buildspec
imgformat containerd-accessstub containerd-mount""".split()
HELPERS = ['container-core', 'qvm', 'zstd']
EXTRA_EXAMPLES = ['container-core', 'qvm-core', 'zstd-raw']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitizers', action='store_true')
    parser.add_argument('-j', '--jobs', type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    cc = shlex.split(os.environ.get('CC', 'cc'))
    cxx = shlex.split(os.environ.get('CXX', 'c++'))
    if not cc or not shutil.which(cc[0]):
        parser.error('C compiler unavailable')
    have_cxx = bool(cxx and shutil.which(cxx[0]))
    flags = ['-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG',
             '-I' + str(ROOT / 'include')]
    links = ['-lm'] if os.name != 'nt' else []
    if os.name == 'posix':
        flags += ['-pthread']
        links += ['-ldl'] if os.uname().sysname in ('Linux', 'FreeBSD') else []
    if args.sanitizers:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                  '-fno-omit-frame-pointer']
    texts = {p.name: p.read_text() for p in (ROOT / 'include').glob('*.h')}
    switch_pattern = re.compile(r'^#\s*if(?:def\s+|\s+defined\s*\(\s*)'
                                r'(MULIB_IMPL_[A-Z0-9_]+)', re.M)
    failures = []
    for name in MODULES:
        for relative in ('include/mulib-' + name + '.h', 'manual/mulib-' + name + '.md',
                         'examples/modules/' + name + '.c'):
            if not (ROOT / relative).is_file() or not (ROOT / relative).stat().st_size:
                failures.append((relative, 'missing/empty module artifact'))
    for name in HELPERS:
        for relative in ('include/include-mulib-' + name + '.h',
                         'manual/include-mulib-' + name + '.md'):
            if not (ROOT / relative).is_file():
                failures.append((relative, 'missing dependency artifact'))

    def closure(name, found=None):
        found = set() if found is None else found
        if name in found:
            return found
        if name not in texts:
            raise ValueError('missing dependency ' + name)
        found.add(name)
        for dep in re.findall(r'^\s*#\s*include\s*"((?:include-)?mulib-[^"]+\.h)"',
                              texts[name], re.M):
            closure(dep, found)
        return found

    def defines(names):
        switches = set()
        for name in names:
            match = switch_pattern.search(texts[name])
            if match:
                switches.add(match.group(1))
        if 'mulib-parsec.h' in names:
            switches.add('MULIB_IMPL_PARSER')
        return ''.join('#define ' + switch + '\n' for switch in sorted(switches))

    cases = []
    headers = ['mulib-' + name + '.h' for name in MODULES]
    headers += ['include-mulib-' + name + '.h' for name in HELPERS]
    all_deps = set()
    for name in headers:
        dependencies = closure(name)
        all_deps.update(dependencies)
        include = ('#include "' + name + '"\n') * 2
        main_source = 'int main(void) { return 0; }\n'
        cases.append((name + ' C declarations', include + main_source, None, 'c'))
        if have_cxx:
            cases.append((name + ' C++ declarations', include + main_source, None, 'c++'))
        cases.append((name + ' implementation', defines(dependencies) + include + main_source, None, 'c'))
    # Late switches explicitly reinclude dependencies as well as public roots.
    combined_headers = sorted(all_deps)
    for order in (combined_headers, list(reversed(combined_headers))):
        includes = ''.join('#include "' + name + '"\n' for name in order)
        cases.append(('combined ' + order[0], defines(all_deps) + includes + main_source, None, 'c'))
        cases.append(('late implementations ' + order[0], includes + defines(all_deps) + includes + main_source, None, 'c'))
    for name in MODULES + EXTRA_EXAMPLES:
        path = ROOT / 'examples/modules' / (name + '.c')
        cases.append(('example ' + name, None, path, 'c'))
    for name in ('test_container_stack', 'test_container_alloc', 'test_container_backends',
                 'test_container_wire'):
        cases.append((name, None, ROOT / 'tests' / (name + '.c'), 'c'))

    parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='mulib-containers-', dir=parent) as temp:
        work = Path(temp)

        def check(case):
            label, source, path, language = case
            with tempfile.TemporaryDirectory(dir=work) as directory:
                binary = Path(directory) / 'test'
                command = (cc if language == 'c' else cxx) + [
                    '-std=c11' if language == 'c' else '-std=c++17'] + flags
                command += ['-x', language, '-'] if source is not None else [str(path)]
                command += links + ['-o', str(binary)]
                try:
                    built = subprocess.run(command, input=source, text=True, capture_output=True,
                                           cwd=directory, timeout=180)
                    if built.returncode:
                        return label, built.stderr or built.stdout
                    ran = subprocess.run([str(binary)], text=True, capture_output=True,
                                         cwd=directory, timeout=60)
                    if ran.returncode:
                        return label, ran.stderr or ran.stdout or str(ran.returncode)
                    if label == 'example container-packzstd' and shutil.which('zstd'):
                        packed, original = Path(directory) / 'image.zst', Path(directory) / 'image.bin'
                        subprocess.run([str(binary), str(packed), str(original)], check=True,
                                       capture_output=True, timeout=60)
                        decoded = subprocess.run(['zstd', '-q', '-d', '-c', str(packed)],
                                                 check=True, capture_output=True, timeout=60)
                        if decoded.stdout != original.read_bytes():
                            return label, 'external Zstandard roundtrip mismatch'
                    if label == 'example container-packelf' and shutil.which('readelf'):
                        obj = Path(directory) / 'image.o'
                        subprocess.run([str(binary), str(obj)], check=True, capture_output=True, timeout=60)
                        inspected = subprocess.run(['readelf', '-h', '-S', str(obj)],
                                                   check=True, capture_output=True, timeout=60)
                        if b'.mulib.image' not in inspected.stdout or inspected.stderr:
                            return label, 'external ELF section inspection failed'
                except (OSError, subprocess.SubprocessError) as error:
                    return label, str(error)
            return None

        with ThreadPoolExecutor(max_workers=args.jobs) as executor:
            results = list(executor.map(check, cases))
        failures.extend(result for result in results if result)
    for label, detail in failures:
        print('FAILED ' + label + ':\n' + detail)
    print(f'Container stack: {len(cases) - sum(bool(r) for r in results)}/{len(cases)} checks passed; '
          f'{len(MODULES)} module manuals, {len(HELPERS)} dependency manuals')
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
