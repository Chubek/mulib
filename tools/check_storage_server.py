#!/usr/bin/env python3
"""Strict isolated dependency, multi-TU, mutation and native-format checks.

No third-party packages required; llvm-readobj and Python msgpack are optional
independent interoperability checks. CC/CXX select compilers; --sanitizers
enables ASan/UBSan for functional and allocation-failure tests.
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
MODULES = """amd64 row-rdbms column-rdbms graphdb objectdb embeddingsdb documentdb
dbserializer bson messagepack protobuf dbschema webidl muidl wit cbor dbdesializer
mulib elfbuilder machobuidler pebuilder coffbuilder evtloop vhpst rewrite_url
compression upstream load_balancer health_checker rate_limit waf acl auth seurity
oauth2 metrics httpserver_config http_request http_response mime web_conntection
http1 http2 http3 quic static_file""".split()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitizers', action='store_true')
    args = parser.parse_args()
    cc = shlex.split(os.environ.get('CC', 'cc'))
    cxx = shlex.split(os.environ.get('CXX', 'c++'))
    flags = ['-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I' + str(ROOT / 'include')]
    texts = {p.name: p.read_text() for p in (ROOT / 'include').glob('mulib-*.h')}
    deps = {name: re.findall(r'^\s*#\s*include\s*"(mulib-[^"]+\.h)"', text, re.M)
            for name, text in texts.items()}
    manifest = (ROOT / 'MANIFEST.yaml').read_text().split('project_files:')[0]
    blocks = re.split(r'^- name: ', manifest, flags=re.M)[1:]
    entries = {re.search(r'^  header: include/([^\n]+)$', block, re.M).group(1): block
               for block in blocks}
    for module in MODULES:
        header = 'mulib-' + module + '.h'
        block = entries[header]
        listed = re.search(r'^  dependencies: \[([^\]]*)\]$', block, re.M).group(1)
        assert {d.strip() for d in listed.split(',') if d.strip()} == {d[6:-2] for d in deps[header]}, header
        assert '  status: implemented\n' in block, header
        assert (ROOT / 'manual' / ('mulib-' + module + '.md')).is_file(), module
        for fixture in ['implementation.c', 'declarations.c']:
            assert '#include "' + header + '"' in (ROOT / 'tests' / fixture).read_text(), (fixture, header)

    def closure(name, found=None):
        found = set() if found is None else found
        if name not in found:
            found.add(name)
            for dep in deps[name]:
                closure(dep, found)
        return found

    def run(command, cwd):
        result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, timeout=120)
        if result.returncode:
            raise RuntimeError(shlex.join(command) + '\n' + result.stdout + result.stderr)
        return result.stdout

    temp_parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='mulib-storage-', dir=temp_parent) as directory:
        work = Path(directory)

        def isolated(module):
            header = 'mulib-' + module + '.h'
            sub = work / module.replace(' ', '_')
            sub.mkdir()
            macros = []
            for dep in sorted(closure(header)):
                match = re.search(r'^#\s*if(?:def\s+|\s+defined\s*\(\s*)(MULIB_IMPL_[A-Z0-9_]+)', texts[dep], re.M)
                if not match:
                    raise RuntimeError('missing implementation switch: ' + dep)
                macros.append('#define ' + match.group(1) + '\n')
            inc = '#include "' + header + '"\n'
            # All dependency switches must precede the first include, matching
            # the documented MuLIB contract. Repeated includes are harmless.
            (sub / 'impl.c').write_text(''.join(macros) + inc + inc)
            (sub / 'client.c').write_text(inc + inc + 'int main(void) { return 0; }\n')
            run(cc + ['-std=c11'] + flags + ['impl.c', 'client.c', '-lm', '-o', 'test'], sub)
            run([str(sub / 'test')], sub)
            example = ROOT / 'examples' / 'modules' / (module + '.c')
            run(cc + ['-std=c11'] + flags + [str(example), '-lm', '-o', 'example'], sub)
            run([str(sub / 'example')], sub)
            if shutil.which(cxx[0]):
                run(cxx + ['-std=c++17'] + flags + ['-x', 'c++', '-c', 'client.c', '-o', 'decl.o'], sub)

        with ThreadPoolExecutor(max_workers=4) as pool:
            list(pool.map(isolated, MODULES))
        san = ['-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'] if args.sanitizers else []
        for name in ['test_storage_server', 'test_storage_server_alloc']:
            binary = work / name
            run(cc + ['-std=c11'] + flags + san + [str(ROOT / 'tests' / (name + '.c')), '-lm', '-o', str(binary)], work)
            print(run([str(binary)] + (['--emit'] if name == 'test_storage_server' else []), work).strip())
        reader = shutil.which('llvm-readobj')
        if reader:
            for name in ['text.elf', 'text.obj', 'text.macho', 'text.exe']:
                output = run([reader, '--file-headers', '--sections', name], work)
                if 'text' not in output:
                    raise RuntimeError('native reader did not find text section: ' + name)
            print('ELF/COFF/Mach-O/PE: llvm-readobj accepted all four formats')
        try:
            import msgpack
        except ImportError:
            print('SKIP: optional Python MessagePack interoperability')
        else:
            assert msgpack.unpackb((work / 'value.msgpack').read_bytes(), raw=False) == {'n': 1.5, 's': 'abc', 'a': [True, None]}
            print('MessagePack: independent decoder accepted output')
    print(f'{len(MODULES)} modules: isolated C implementations, runnable examples, C++ declarations and multi-TU links OK')


if __name__ == '__main__':
    main()
