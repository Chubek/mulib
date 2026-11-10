#!/usr/bin/env python3
"""Check compiler modules, independent implementation TUs, regressions and examples."""
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
MODULES = """
branch-layout bytecode bytecode-builder bytecode-disasm bytecode-eval bytecode-opt
bytecode-serialize bytecode-verify callconv cfg compiler-cache compiler-driver
compiler-scope compiler-test consteval constraints dce defuse diagnostic dominance
evtstream frame-layout gc gc-roots graph gvn importgraph ir-print ir-verify lexer
lexgen lexical-token listener liveintervals liveness loops module-resolver
name-resolution parser-pratt passmanager physreg postdominance range-analysis
regalloc regalloc-graph regalloc-linear regpressure search sort source-manager
source-map stackmap symboltable syntax-tree typecheck typesystem visitor vm-host
vm-module vm-value
""".split()


def run(command, cwd):
    result = subprocess.run([str(x) for x in command], cwd=cwd, capture_output=True,
                            text=True, timeout=180)
    if result.returncode:
        raise RuntimeError(shlex.join([str(x) for x in command]) + '\n' +
                           result.stdout + result.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default=os.environ.get('CC', 'cc'))
    parser.add_argument('--cxx', default=os.environ.get('CXX', 'c++'))
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--sanitizers', action='store_true')
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    cc, cxx = shlex.split(args.cc), shlex.split(args.cxx)
    flags = ['-Wall', '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG',
             '-I' + str(ROOT / 'include'), '-O1', '-g']
    if args.sanitizers:
        flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                  '-fno-omit-frame-pointer']
    manifest = (ROOT / 'MANIFEST.yaml').read_text()
    names = re.findall(r'^- name: (.+)$', manifest, re.M)
    assert len(names) == len(set(names)), 'duplicate manifest names'
    for field in ('module_count', 'implemented_module_count'):
        assert int(re.search(r'^' + field + r': (\d+)$', manifest, re.M)[1]) == len(names)
    for name in MODULES:
        assert name in names, 'missing manifest entry: ' + name
        for directory, filename in [('include', 'mulib-' + name + '.h'),
                                    ('manual', 'mulib-' + name + '.md'),
                                    ('examples/modules', name + '.c')]:
            assert (ROOT / directory / filename).is_file(), 'missing ' + filename
    parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='mulib-compiler-', dir=parent) as temporary:
        work = Path(temporary)

        def implementation(name):
            source = work / (name + '.c')
            macro = 'MULIB_IMPL_' + name.upper().replace('-', '_')
            header = '"mulib-' + name + '.h"'
            # Declaration-first and repeated implementation inclusion are supported.
            source.write_text('#include ' + header + '\n#define ' + macro + '\n' +
                              '#include ' + header + '\n#include ' + header + '\n')
            obj = work / (name + '.o')
            run(cc + ['-std=c11'] + flags + ['-c', source, '-o', obj], work)
            return obj

        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            objects = list(pool.map(implementation, MODULES + ['ast', 'strbuf']))

            def regression(source):
                consumer = work / source.name
                executable = work / source.stem
                if source.name == 'test-compiler-allocation.c':
                    run(cc + ['-std=c11'] + flags + [source, '-lm', '-o', executable], work)
                    run([executable], work)
                    return
                text = re.sub(r'^#define MULIB_IMPL_.*\n', '', source.read_text(), flags=re.M)
                consumer.write_text('#include <stdlib.h>\n#include <string.h>\n#include <stdio.h>\n' + text)
                run(cc + ['-std=c11'] + flags + [consumer] + objects + ['-lm', '-o', executable], work)
                run([executable], work)

            suites = sorted((ROOT / 'tests').glob('test-compiler-*.c'))
            list(pool.map(regression, suites))

            def example(name):
                executable = work / ('example-' + name)
                run(cc + ['-std=c11'] + flags + [ROOT / 'examples/modules' / (name + '.c'),
                                                '-lm', '-o', executable], work)
                run([executable], work)

            list(pool.map(example, MODULES))

        if shutil.which(cxx[0]):
            consumer = work / 'consumer.cpp'
            consumer.write_text(''.join('#include "mulib-' + n + '.h"\n' for n in MODULES) + '''
#include <cassert>
int main() {
    mulib_bc_instruction_t code[] = {{MULIB_BC_PUSH, 6}, {MULIB_BC_PUSH, 7},
                                     {MULIB_BC_MUL, 0}, {MULIB_BC_HALT, 0}};
    size_t count=4, folds=0;
    assert(mulib_bytecode_optimize(code, &count, &folds) && folds==1);
    mulib_bc_result_t result;
    assert(mulib_bytecode_eval(code, count, 0, 2, &result) && result.value==42);
    mulib_cfg_t graph{};
    graph.blocks=2;
    assert(mulib_cfg_add(&graph, 0, 1));
    mulib_dominance_t dom;
    assert(mulib_dominance_compute(&graph, 0, &dom) && dom.idom[1]==0);
    mulib_cfg_free(&graph);
}
''')
            executable = work / 'consumer-cpp'
            run(cxx + ['-std=c++17'] + flags + [consumer] + objects + ['-lm', '-o', executable], work)
            run([executable], work)
            print('C++17 declarations and C linkage passed')
        else:
            print('SKIP: C++ compiler unavailable')
    print(f'{len(MODULES)} independent modules, {len(suites)} regression suites and '
          f'{len(MODULES)} examples passed')


if __name__ == '__main__':
    main()
