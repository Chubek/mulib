#!/usr/bin/env python3
"""Verify inventory, isolated headers, composition, examples, and focused tests.

Run from any directory. CC/CXX select compilers; --sanitizers adds ASan/UBSan.
No third-party Python packages are required. All binaries/data are temporary.
"""
import argparse
import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

root = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitizers', action='store_true')
    parser.add_argument('--no-isolation', action='store_true',
                        help='skip per-header compilation, keeping combined checks')
    parser.add_argument('-j', '--jobs', type=int, default=min(4, os.cpu_count() or 1),
                        help='number of independent compiler/test jobs (default: up to 4)')
    parser.add_argument('--compile-timeout', type=int, default=90,
                        help='compiler timeout per build in seconds; increase for large combined TUs')
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    if args.compile_timeout < 1:
        parser.error('--compile-timeout must be positive')
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    cxx = shlex.split(os.environ.get('CXX', 'c++'))
    if not compiler or not shutil.which(compiler[0]):
        parser.error('C compiler not found; set CC to an available compiler')
    have_cxx = bool(cxx and shutil.which(cxx[0]))
    flags = ['-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG',
             '-I' + str(root / 'kernels')]
    if os.name == 'posix':
        flags += ['-pthread']
    if args.sanitizers:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                  '-fno-omit-frame-pointer']
    links = ([] if sys.platform == 'win32' else ['-lm'])
    if sys.platform.startswith(('linux', 'freebsd')):
        links += ['-ldl']
    headers = sorted((root / 'kernels').glob('mulib-*.h'))
    texts = {p.name: p.read_text() for p in headers}
    switches, dependencies, extra_switches, failures = {}, {}, {}, []
    reserved = set()
    snapshot = subprocess.run([sys.executable, str(root / 'tools/check_snapshot.py')],
                              capture_output=True, text=True)
    if snapshot.returncode:
        failures.append(('dsymspec snapshot', snapshot.stderr or snapshot.stdout))
    else:
        print(snapshot.stdout.strip())
    units = subprocess.run([sys.executable, str(root / 'tools/embed_units.py'), '--check'],
                           capture_output=True, text=True)
    if units.returncode:
        failures.append(('unit profiles', units.stderr or units.stdout))
    else:
        print(units.stdout.strip())
    ldocstr = subprocess.run([sys.executable, str(root / 'tools/embed_ldocstr.py'), '--check'],
                            capture_output=True, text=True)
    if ldocstr.returncode:
        failures.append(('ldocstr modules', ldocstr.stderr or ldocstr.stdout))
    else:
        print(ldocstr.stdout.strip())
    notation = subprocess.run([sys.executable, str(root / 'tools/embed_notation.py'), '--check'],
                             capture_output=True, text=True)
    if notation.returncode:
        failures.append(('notation libraries', notation.stderr or notation.stdout))
    else:
        print(notation.stdout.strip())
    strata = subprocess.run([sys.executable, str(root / 'tools/stratify.py'), '--check'],
                            capture_output=True, text=True)
    if strata.returncode:
        failures.append(('dependency strata', strata.stderr or strata.stdout))
    else:
        print(strata.stdout.strip())
    manuals = subprocess.run([sys.executable, str(root / 'tools/manuals.py'), '--check'],
                             capture_output=True, text=True)
    if manuals.returncode:
        failures.append(('manual references', manuals.stderr or manuals.stdout))
    else:
        print(manuals.stdout.strip())
    mull = subprocess.run([sys.executable, str(root / 'mudsl/scripts/check_mulib_inventory.py'),
                           '--include', str(root / 'kernels')], capture_output=True, text=True)
    if mull.returncode:
        failures.append(('MuLL bindings', mull.stderr or mull.stdout))
    else:
        print(mull.stdout.strip())
    tool_tests = subprocess.run([sys.executable, str(root / 'tests/test_tools.py')],
                                capture_output=True, text=True)
    if tool_tests.returncode:
        failures.append(('tool regressions', tool_tests.stderr or tool_tests.stdout))
    kernelbind = subprocess.run([sys.executable, str(root / 'tools/extend-kernelbind.py'), '--check'],
                               capture_output=True, text=True)
    if kernelbind.returncode:
        failures.append(('On1x KernelBind inventory', kernelbind.stderr or kernelbind.stdout))
    else:
        print(kernelbind.stdout.strip())
    manifest = (root / 'MANIFEST.yaml').read_text()
    entries = {}
    for block in re.split(r'^- name: ', manifest.split('project_files:')[0], flags=re.M)[1:]:
        header = re.search(r'^  header: kernels/(mulib-[^\n]+\.h)$', block, re.M)
        if not header:
            failures.append(('MANIFEST.yaml', 'module entry has no header'))
            continue
        name = header.group(1)
        if name in entries:
            failures.append(('MANIFEST.yaml', 'duplicate header entry ' + name))
        entries[name] = block
    if set(entries) != set(texts):
        failures.append(('MANIFEST.yaml', 'module entries do not match the include inventory'))
    count = re.search(r'^module_count: (\d+)$', manifest, re.M)
    if not count or int(count.group(1)) != len(headers):
        failures.append(('MANIFEST.yaml', 'module_count does not match header inventory'))
    bounded, engineering, optional = set(), set(), set()
    for header in headers:
        name, text = header.name, texts[header.name]
        match = re.search(r'^#\s*if(?:def\s+|\s+defined\s*\(\s*)'
                          r'(MULIB_IMPL_[A-Z0-9_]+)', text, re.M)
        block = entries.get(name, '')
        if re.search(r'^  optional_dependency: .+$', block, re.M):
            optional.add(name)
        profile = re.search(r'^  profile: engineering$', block, re.M)
        if profile:
            engineering.add(name)
            for key in ('manual', 'example'):
                field = re.search(r'^  ' + key + r': (.+)$', block, re.M)
                if (field and not (root / field.group(1)).is_file()) or (key == 'manual' and not field):
                    failures.append((name, 'missing engineering ' + key))
        if re.search(r'^  implementation: (?:static-inline|static-data)$', block, re.M):
            bounded.add(name)
            if match or not re.search(r'^  status: bounded$', block, re.M):
                failures.append((name, 'non-external module must have bounded status and no ABI switch'))
            dependencies[name] = [d for d in re.findall(r'^\s*#\s*include\s*["<]'
                                      r'(mulib-[^">]+\.h)[">]', text, re.M)]
            deps = re.search(r'^  dependencies: \[([^\]]*)\]$', block, re.M)
            actual = {d.strip() for d in deps.group(1).split(',') if d.strip()} if deps else None
            expected = {d[len('mulib-'):-len('.h')] for d in dependencies[name]}
            if actual != expected:
                failures.append((name, 'manifest dependency list differs from header includes'))
            if not profile:
                for key in ('manual', 'example'):
                    field = re.search(r'^  ' + key + r': (.+)$', block, re.M)
                    if not field or not (root / field.group(1)).is_file():
                        failures.append((name, 'missing bounded module ' + key))
            for dependency in dependencies[name]:
                if dependency not in texts:
                    failures.append((name, 'unknown MuLIB dependency ' + dependency))
            continue
        if re.search(r'^  status: reserved$', block, re.M):
            # Explicitly inventoried future modules are not implemented APIs.
            # They must remain empty/comment-only, and cannot be dependencies.
            remaining = re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S).strip()
            if remaining:
                failures.append((name, 'reserved header contains code; implement and register it'))
            reserved.add(name)
            continue
        if not match:
            failures.append((name, 'missing implementation switch or empty header'))
            continue
        if not re.search(r'^  status: implemented$', block, re.M):
            failures.append((name, 'implemented header must have status: implemented'))
        switches[name] = match.group(1)
        dependencies[name] = [d for d in re.findall(r'^\s*#\s*include\s*["<]'
                                        r'(mulib-[^">]+\.h)[">]', text, re.M)]
        module = name[len('mulib-'):-len('.h')]
        example = re.search(r'^  example: (.+)$', block, re.M)
        if not profile:
            for expected in ['manual/mulib-' + module + '.md',
                             example.group(1) if example else 'examples/modules/' + module + '.c']:
                if not (root / expected).is_file():
                    failures.append((name, 'missing ' + expected))
        if name not in entries:
            failures.append((name, 'missing manifest entry'))
        else:
            block = entries[name]
            extras = re.search(r'^  extra_macros: \[([^\]]*)\]$', block, re.M)
            extra_switches[name] = re.findall(r'MULIB_IMPL_[A-Z0-9_]+', extras.group(1)) if extras else []
            macro = re.search(r'^  macro: (MULIB_IMPL_[A-Z0-9_]+)$', block, re.M)
            if not macro or macro.group(1) != switches[name]:
                failures.append((name, 'manifest implementation switch mismatch'))
            deps = re.search(r'^  dependencies: \[([^\]]*)\]$', block, re.M)
            actual = {d.strip() for d in deps.group(1).split(',') if d.strip()} if deps else None
            expected = {d[len('mulib-'):-len('.h')] for d in dependencies[name]}
            if actual != expected:
                failures.append((name, 'manifest dependency list differs from header includes'))
        for dependency in dependencies[name]:
            if dependency not in texts:
                failures.append((name, 'unknown MuLIB dependency ' + dependency))

    for name, deps in dependencies.items():
        for dependency in deps:
            if dependency in reserved:
                failures.append((name, 'depends on reserved header ' + dependency))
    implemented_count = re.search(r'^implemented_module_count: (\d+)$', manifest, re.M)
    if not implemented_count or int(implemented_count.group(1)) != len(switches):
        failures.append(('MANIFEST.yaml', 'implemented_module_count does not match implemented headers'))
    bounded_count = re.search(r'^bounded_module_count: (\d+)$', manifest, re.M)
    if bounded and (not bounded_count or int(bounded_count.group(1)) != len(bounded)):
        failures.append(('MANIFEST.yaml', 'bounded_module_count does not match non-external headers'))

    if len(set(switches.values())) != len(switches):
        failures.append(('headers', 'implementation switch shared by multiple modules'))
    implementation = (root / 'tests/implementation.c').read_text()
    declarations = (root / 'tests/declarations.c').read_text()
    for name in texts:
        if name in bounded or name in engineering:
            continue  # graphics gate or generated engineering compilation below
        for path, text in [('tests/implementation.c', implementation),
                           ('tests/declarations.c', declarations)]:
            if '#include "' + name + '"' not in text:
                failures.append((path, 'missing header ' + name))
    for name, switch in switches.items():
        if name not in engineering | optional and '#define ' + switch + '\n' not in implementation:
            failures.append(('tests/implementation.c', 'missing switch ' + switch))

    def closure(name, seen=None):
        seen = set() if seen is None else seen
        if name in seen:
            return seen
        seen.add(name)
        for dependency in dependencies.get(name, []):
            if dependency in texts:
                closure(dependency, seen)
        return seen

    temp_parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    counts = {}
    with tempfile.TemporaryDirectory(prefix='mulib-check-', dir=temp_parent) as tmp:
        tmp = Path(tmp)

        def check(label, sources=None, source_text=None, language='c', execute=True, extra_flags=None):
            if sources == [root / 'tests/test_sandbox.c']:
                if sys.platform != 'linux' or platform.machine() not in ('x86_64', 'aarch64'):
                    print('SKIP sandbox process fixtures: requires native Linux x86-64/AArch64')
                    return True
                env = dict(os.environ, CC=shlex.join(compiler), SANITIZE=str(int(args.sanitizers)))
                try:
                    result = subprocess.run(['sh', 'tests/run_sandbox.sh'], cwd=root,
                                            env=env, capture_output=True, text=True, timeout=60)
                except (OSError, subprocess.TimeoutExpired) as error:
                    failures.append((label, str(error)))
                    return False
                if result.returncode:
                    failures.append((label, result.stdout + result.stderr))
                    return False
                print(result.stdout.rstrip(), flush=True)
                return True
            with tempfile.TemporaryDirectory(prefix='case-', dir=tmp) as work:
                binary = Path(work) / 'test'
                source_name = sources[0].name if sources and len(sources) == 1 else ''
                compile_only = source_name == 'test_mune_compile.c'
                command = (compiler if language == 'c' else cxx) + [
                    '-std=c11' if language == 'c' else '-std=c++17'] + flags
                command += extra_flags or []
                if source_name == 'sandbox-process.c' and args.sanitizers:
                    # The trusted-bootstrap example also becomes the strict worker.
                    command += ['-fno-sanitize=address,undefined']
                # Match CMake's shell-test profile; test_mune_compile deliberately
                # checks real API declarations without linking Mune implementations.
                if os.name == 'posix' and re.match(r'^test_(parallel|shebang|sh_|mune_compile)', source_name):
                    command += ['-D_POSIX_C_SOURCE=200809L']
                if sources and len(sources) == 1 and sources[0].name == 'test_android_sdk.c':
                    command += ['-I' + str(root / 'tests/fixtures/android-sdk')]
                if sources and len(sources) == 1 and sources[0].name == 'test_mune_runtime.c' and sys.platform.startswith(('linux', 'freebsd')):
                    command += ['-rdynamic']
                if source_text is not None:
                    command += ['-x', language, '-']
                else:
                    if language != 'c':
                        command += ['-x', language]
                    command += [str(p) for p in sources]
                command += (['-c'] if compile_only else links) + ['-o', str(binary)]
                try:
                    build = subprocess.run(command, input=source_text, cwd=work,
                                           capture_output=True, text=True, timeout=args.compile_timeout)
                    if build.returncode:
                        failures.append((label, build.stderr or build.stdout))
                        return False
                    if execute and not compile_only:
                        arguments = ([str(root / 'library')] if sources and len(sources) == 1
                                      and sources[0].name in ('immgtk.c', 'test_immgtk.c') else [])
                        if source_name in ('test_imtui_mune.c', 'test-elvi-scrape.c',
                                            'test-elvi-scrape-providers.c', 'test-elvi-script.c'):
                            arguments = [str(root)]
                        if source_name == 'test-elvi-script-logic.c':
                            arguments = [str(root / 'library/elvis/logic/BookingDotCom.lua')]
                        expected_output = None
                        if sources == [root / 'examples/parallel.c']:
                            arguments = ['-k', "printf '%s\\n' {}", ':::', 'alpha', 'beta']
                            expected_output = 'alpha\nbeta\n'
                        elif sources == [root / 'examples/shebang.c']:
                            script = Path(work) / 'input.txt'
                            script.write_text('hello ${1}\n')
                            arguments = [str(script), 'world']
                            expected_output = 'hello world\n'
                        elif sources == [root / 'examples/apply_unidiff.c']:
                            target = Path(work) / 'hello.txt'
                            target.write_bytes(b'old\n')
                            patch = Path(work) / 'change.diff'
                            patch.write_bytes(b'--- a/hello.txt\n+++ b/hello.txt\n'
                                              b'@@ -1 +1 @@\n-old\n+new\n')
                            arguments = [work, str(patch)]
                        elif sources == [root / 'examples/elvi-scrape.c']:
                            arguments = [str(root / 'library/elvis/Google.elvs'), 'offline']
                        if sources and len(sources) == 1 and sources[0].name == 'build-publication.c':
                            arguments = ['.']
                        test = subprocess.run([str(binary)] + arguments, cwd=work, capture_output=True,
                                              text=True, timeout=60)
                        if test.returncode:
                            failures.append((label, test.stderr or test.stdout or
                                              'exit status ' + str(test.returncode)))
                            return False
                        if sources == [root / 'examples/apply_unidiff.c'] and target.read_bytes() != b'new\n':
                            failures.append((label, 'expected the example to replace the file'))
                            return False
                        if expected_output is not None and test.stdout != expected_output:
                            failures.append((label, 'unexpected CLI output: ' + repr(test.stdout)))
                            return False
                        if sources and len(sources) == 1 and sources[0].name == 'build-publication.c':
                            formats = subprocess.run(
                                [sys.executable, str(root / 'tests/verify-formats.py'), work],
                                cwd=work, capture_output=True, text=True, timeout=60)
                            if formats.returncode:
                                failures.append((label + ' formats', formats.stderr or formats.stdout))
                                return False
                        if sources and len(sources) == 1 and sources[0].name in (
                                'test_mir_flow.c', 'test_compiler_ir.c'):
                            aot = subprocess.run(
                                [sys.executable, str(root / 'tools/check_mir_aot.py'),
                                 '--emitter', str(binary), '--compiler', shlex.join(compiler)]
                                + (['--sanitizers'] if args.sanitizers else []),
                                cwd=work, capture_output=True, text=True, timeout=180)
                            if aot.returncode:
                                failures.append((label + ' AOT', aot.stderr or aot.stdout))
                                return False
                        if sources and len(sources) == 1 and sources[0].name == 'test_mune_runtime.c':
                            runtime = subprocess.run(
                                [sys.executable, str(root / 'tools/check_mune_runtime.py'),
                                 '--runtime', str(binary), '--compiler', shlex.join(compiler)]
                                + (['--sanitizers'] if args.sanitizers else []),
                                cwd=work, capture_output=True, text=True, timeout=300)
                            if runtime.returncode:
                                failures.append((label + ' backends/reference', runtime.stderr or runtime.stdout))
                                return False
                        if sources and len(sources) == 1 and sources[0].name == 'test_docstring.c':
                            command = [sys.executable, str(root / 'tools/check_docstring.py'),
                                       '--runtime', str(binary), '--library', str(root / 'library/ldocstr')]
                            if shutil.which('pandoc'):
                                command += ['--pandoc', shutil.which('pandoc')]
                            interop = subprocess.run(command, cwd=work, capture_output=True, text=True, timeout=180)
                            if interop.returncode:
                                failures.append((label + ' interoperability', interop.stderr or interop.stdout))
                                return False
                        if sources and len(sources) == 1 and sources[0].name == 'test_compression_codecs.c':
                            interop = subprocess.run(
                                [sys.executable, str(root / 'tools/check_compression.py'), '--codec', str(binary)],
                                cwd=work, capture_output=True, text=True, timeout=180)
                            if interop.returncode:
                                failures.append((label + ' interoperability', interop.stderr or interop.stdout))
                                return False
                        if sources and len(sources) == 1 and sources[0].name == 'test_notation.c':
                            command = [sys.executable, str(root / 'tools/check_notation.py'), '--runtime', str(binary)]
                            for program, option in [('rsvg-convert', '--rsvg'), ('pdflatex', '--pdflatex'), ('lualatex', '--lualatex')]:
                                if shutil.which(program):
                                    command += [option, shutil.which(program)]
                            interop = subprocess.run(command, cwd=work, capture_output=True, text=True, timeout=240)
                            if interop.returncode:
                                failures.append((label + ' interoperability', interop.stderr or interop.stdout))
                                return False
                        if sources and len(sources) == 1 and sources[0].name == 'test_dynbox.c':
                            abi = subprocess.run(
                                [sys.executable, str(root / 'tools/check_dynbox_abi.py'),
                                 '--compiler', shlex.join(compiler)]
                                + (['--sanitizers'] if args.sanitizers else []),
                                cwd=work, capture_output=True, text=True, timeout=240)
                            if abi.returncode:
                                failures.append((label + ' compiler ABI', abi.stderr or abi.stdout))
                                return False
                        if sources and len(sources) == 1 and sources[0].name == 'test_microcc.c':
                            native = subprocess.run(
                                [sys.executable, str(root / 'tools/check_microcc_native.py'),
                                 '--emitter', str(binary), '--compiler', shlex.join(compiler)]
                                + (['--sanitizers'] if args.sanitizers else []),
                                cwd=work, capture_output=True, text=True, timeout=180)
                            if native.returncode:
                                failures.append((label + ' native assembly', native.stderr or native.stdout))
                                return False
                        if sources and len(sources) == 1 and sources[0].name == 'test_metapp.c':
                            metapp = subprocess.run(
                                [sys.executable, str(root / 'tools/check_metapp.py'),
                                 '--preprocessor', str(binary), '--compiler', shlex.join(compiler)],
                                cwd=work, capture_output=True, text=True, timeout=180)
                            if metapp.returncode:
                                failures.append((label + ' preprocessor/plugins', metapp.stderr or metapp.stdout))
                                return False
                        if sources and len(sources) == 1 and sources[0].name == 'test_asdl.c':
                            generated = subprocess.run(
                                [sys.executable, str(root / 'tools/check_asdl.py'),
                                 '--emitter', str(binary), '--compiler', shlex.join(compiler)]
                                + (['--sanitizers'] if args.sanitizers else []),
                                cwd=work, capture_output=True, text=True, timeout=60)
                            if generated.returncode:
                                failures.append((label + ' generated C99', generated.stderr or generated.stdout))
                                return False
                        if sources and len(sources) == 1 and sources[0].name == 'test_cassowary_solver.c':
                            reference = subprocess.run(
                                [sys.executable, str(root / 'tools/check_cassowary.py'), '--solver', str(binary)],
                                cwd=work, capture_output=True, text=True, timeout=60)
                            if reference.returncode:
                                failures.append((label + ' rational reference', reference.stderr or reference.stdout))
                                return False
                        if sources and len(sources) == 1 and sources[0].name == 'test_xll.c':
                            generated = subprocess.run(
                                [sys.executable, str(root / 'tools/check_xll.py'),
                                 '--emitter', str(binary), '--compiler', shlex.join(compiler)]
                                + (['--sanitizers'] if args.sanitizers else []),
                                cwd=work, capture_output=True, text=True, timeout=300)
                            if generated.returncode:
                                failures.append((label + ' generated C', generated.stderr or generated.stdout))
                                return False
                except (OSError, subprocess.TimeoutExpired) as error:
                    failures.append((label, str(error)))
                    return False
                return True

        def check_many(cases):
            with ThreadPoolExecutor(max_workers=args.jobs) as executor:
                return sum(executor.map(lambda case: check(**case), cases))

        api_kernel_tests = {'test-api-json-http.c', 'test-api-sse-client.c', 'test-api-websocket.c'}
        api_kernel_sources = [root / 'tests/api-kernels' / name
                              for name in ('implementation.c', 'dependencies.c')]
        focused_peers = {name: api_kernel_sources for name in api_kernel_tests}
        focused_peers['test_raytracing.c'] = [root / 'tests/raytracing-implementation.c']
        for category, sources in [
            ('module examples', sorted((root / 'examples/modules').glob('*.c'))),
            ('composition examples', sorted((root / 'examples').glob('*.c'))),
            # SSH fixtures use separately compiled kernels and optional libcrypto.
            # Run them with make check-ssh or opt in to the CMake SSH profile.
            ('focused tests', sorted(p for p in (root / 'tests').glob('test_*.c')
                                     if not p.name.startswith('test_ssh_'))
             + sorted((root / 'tests').glob('test-*.c'))),
        ]:
            passed = check_many([
                 dict(label=str(p.relative_to(root)),
                      sources=[p] + focused_peers.get(p.name, []),
                      # The focused CMake/Make integration runs this program
                      # with an inherited authenticated-by-host Unix stream.
                      execute=p.name != 'elvi-script-receiver.c')
                 for p in sources])
            counts[category] = (passed, len(sources))
            print(f'MuLIB {category}: {passed}/{len(sources)} passed', flush=True)
        integration = [
            ('smoke', [root / 'tests/smoke.c']),
            ('CDD semantics', [root / 'tests/cdd-test.c', root / 'tests/cdd-implementation.c']),
            ('functional', [root / 'tests/implementation.c', root / 'tests/functional.c']),
            ('composition', [root / 'tests/implementation.c', root / 'tests/composition.c']),
            ('extensions', [root / 'tests/implementation.c', root / 'tests/extensions.c']),
            ('all implementations', [root / 'tests/all_headers.c']),
            ('C declarations', [root / 'tests/declarations.c']),
        ]
        cases = [dict(label=label, sources=paths) for label, paths in integration]

        def header_switches(names):
            enabled = {switches[name] for name in names if name in switches}
            for name in names:
                enabled.update(extra_switches.get(name, []))
            return ''.join('#define ' + switch + '\n' for switch in sorted(enabled))

        # The shell modules require this before any system header, including in
        # the declaration-first and reverse-order combined implementation tests.
        posix_source = '#ifndef _POSIX_C_SOURCE\n#define _POSIX_C_SOURCE 200809L\n#endif\n'
        # Optional backends still participate in declaration checks; their
        # implementations are exercised by dependency-aware opt-in targets.
        defines = header_switches(texts.keys() - engineering - optional)
        includes = ''.join('#include "' + header.name + '"\n' for header in reversed(headers)
                           if header.name not in engineering)
        main_source = 'int main(void) { return 0; }\n'
        linux_source = ('#if defined(__linux__) && !defined(_GNU_SOURCE)\n'
                        '#define _GNU_SOURCE\n#endif\n')
        combined_source = linux_source + posix_source + '#include <stdlib.h>\n#undef bsearch\n'
        cases.append(dict(label='reverse-order implementations',
                          source_text=combined_source + defines + includes + main_source))
        cases.append(dict(label='implementations after declarations',
                          source_text=combined_source + includes + defines + includes + main_source))
        if os.name == 'posix':
            cases.append(dict(label='POSIX shell', sources=[root / 'tests/test_runtime.c'],
                              extra_flags=['-DMULIB_MINISH_ENABLE_POSIX']))
        if have_cxx:
            cases.append(dict(label='C++ declarations', sources=[root / 'tests/declarations.c'],
                              language='c++'))
        else:
            print('C++ declaration check skipped: no C++ compiler')
        # engineering includes a resolver symbol incompatible with legacy dsim-logic4.
        # Scope the indentation exception to this separate reference family.
        engineering_flags = ['-Wno-misleading-indentation']
        if engineering:
            includes = ''.join(f'#include "{name}"\n' for name in sorted(engineering))
            dependencies = set().union(*(closure(name) for name in engineering))
            cases.append(dict(label='engineering combined implementations',
                              source_text=header_switches(dependencies) + includes + main_source,
                              extra_flags=engineering_flags))
            if have_cxx:
                cases.append(dict(label='engineering C++ declarations', source_text=includes + main_source,
                                  language='c++', extra_flags=engineering_flags))
        counts['combined builds'] = (check_many(cases), len(cases))
        print('MuLIB combined builds: %s/%s passed' % counts['combined builds'], flush=True)
        if not args.no_isolation:
            cases = []
            for header in headers:
                name = header.name
                extra_flags = engineering_flags if name in engineering else []
                declaration = ('#include "' + name + '"\n') * 2 + 'int main(void) { return 0; }\n'
                cases.append(dict(label='isolated declarations: ' + name, source_text=declaration,
                                  extra_flags=extra_flags))
                if have_cxx:
                    cases.append(dict(label='isolated C++ declarations: ' + name,
                                      source_text=declaration, language='c++', extra_flags=extra_flags))
                if name in reserved or closure(name) & optional:
                    continue
                enabled = set()
                for dependency in closure(name):
                    if dependency in switches:
                        enabled.add(switches[dependency])
                        enabled.update(extra_switches.get(dependency, []))
                defines = ''.join('#define ' + switch + '\n' for switch in sorted(enabled))
                if enabled & {'MULIB_IMPL_PARALLEL_SH', 'MULIB_IMPL_SHEBANG_SH', 'MULIB_IMPL_SECRET_STORE',
                              'MULIB_IMPL_WORKSPACE_FS', 'MULIB_IMPL_WORKSPACE_EDIT'}:
                    defines = posix_source + defines
                if enabled & {'MULIB_IMPL_CGROUP2', 'MULIB_IMPL_IPC_FRAMED',
                              'MULIB_IMPL_LANDLOCK', 'MULIB_IMPL_NAMESPACE',
                              'MULIB_IMPL_SANDBOX_PROCESS', 'MULIB_IMPL_SECCOMP',
                              'MULIB_IMPL_PROCESS_STREAM'}:
                    defines = linux_source + posix_source + defines
                cases.append(dict(label='isolated implementation: ' + name,
                                  source_text=defines + declaration, extra_flags=extra_flags))
            print(f'MuLIB checking {len(cases)} isolated header builds', flush=True)
            counts['isolated headers'] = (check_many(cases), len(cases))
    for category, (passed, total) in counts.items():
        print(f'MuLIB {category}: {passed}/{total} passed')
    print(f'MuLIB inventory: {len(switches)} implemented, {len(reserved)} reserved headers')
    for name, reason in sorted(failures):
        print(f'FAILED {name}:\n{reason}', file=sys.stderr)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
