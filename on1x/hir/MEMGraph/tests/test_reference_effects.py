#!/usr/bin/env python3
"""Milestone B reference baseline: compare effects and loan errors with MEMGraph off/on."""
import argparse
from itertools import product
from pathlib import Path
import subprocess
import sys
import json

from test_integration import Differential, OFF, ON, require_success, run


class EmbeddedDifferential(Differential):
    """Host AMD64 probe of the actual selected Cephyr ABI, outside the On1x CLI."""
    def __init__(self, args):
        super().__init__(args)
        self.root = args.libstone_root.resolve() / 'cornerstone' / 'Cephyr'
        sys.path.insert(0, str(self.root / 'python'))
        import cephyr
        self.compiler_api = cephyr.Compiler(args.cephyr_library.resolve())
        self.config = self.work / 'baseline.toml'
        self.config.write_text('# No default QML profile.\n', encoding='utf-8')

    def compile(self, generated, *options):
        return self.compiler_api.compile(
            ['--config', self.config, '--target=x86_64-linux-gnu', '--diagnostics-format=sarif',
             *options, '-I', self.runtime, '-x', 'c', '-', '-c', '-o', '-'],
            generated.encode('utf-8'), resource_root=self.root, archon_root=self.root.parent / 'Archon')

    def execute(self, source, mode, generated):
        reference = super().execute(source, mode, generated)
        for qml, factir in product((False, True), repeat=2):
            flags = ['--factir' if factir else '--no-factir']
            if qml:
                flags.append('--qml-pass=canonicalize')
            result = self.compile(generated, *flags)
            assert result.ok and result.output.startswith(b'\x7fELF'), result.diagnostics
            assert result.sarif()['version'] == '2.1.0', result.diagnostics
            obj = self.work / f'{source.stem}.{mode}.q{int(qml)}.f{int(factir)}.o'
            obj.write_bytes(result.output)
            executable = obj.with_suffix('')
            require_success(run(*self.cc, '-no-pie', obj, self.runtime / 'on1x_runtime.c',
                                *self.cflags, '-o', executable))
            actual = run(executable)
            assert (actual.returncode, actual.stdout, actual.stderr) == reference, (obj, actual, reference)
        return reference


class DriverDifferential(Differential):
    def execute(self, source, mode, generated):
        reference = super().execute(source, mode, generated)
        for qml, factir in product((False, True), repeat=2):
            stem = self.work / f'{source.stem}.{mode}.q{int(qml)}.f{int(factir)}'
            obj, exe, report = (Path(str(stem) + ext) for ext in ('.o', '.exe', '.sarif'))
            flags = ['--cephyr-factir', 'on' if factir else 'off']
            if qml:
                flags += ['--cephyr-profile', 'scalar', '--cephyr-qml-fuel', '10000',
                          '--cephyr-qml-param', 'canonicalize.fuel=10000']
            require_success(run(self.compiler, '--emit-cephyr-object', source, '-o', obj,
                                *(OFF if mode == 'off' else ON), *flags, '--cephyr-sarif', report))
            document = json.loads(report.read_bytes())
            assert document['version'] == '2.1.0' and not any(
                e['level'] == 'error' for e in document['runs'][0]['results']), document
            require_success(run(*self.cc, obj, self.runtime / 'on1x_runtime.c', *self.cflags, '-o', exe))
            actual = run(exe)
            assert (actual.returncode, actual.stdout, actual.stderr) == reference, (obj, actual)
        return reference


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('on1xc', type=Path)
    parser.add_argument('on1x_source_dir', type=Path)
    parser.add_argument('work_dir', type=Path)
    parser.add_argument('cc')
    parser.add_argument('--cflag', action='append', default=[])
    parser.add_argument('--cephyr-library', type=Path, help='add embedded AMD64 object/link/run comparisons')
    parser.add_argument('--libstone-root', type=Path, help='checkout matching --cephyr-library')
    parser.add_argument('--driver', action='store_true', help='exercise the On1x Cephyr selector')
    args = parser.parse_args()
    if bool(args.cephyr_library) != bool(args.libstone_root):
        parser.error('--cephyr-library and --libstone-root must be used together')
    if args.driver and args.cephyr_library:
        parser.error('select the driver or library probe separately')
    t = (DriverDifferential(args) if args.driver else
         EmbeddedDifferential(args) if args.cephyr_library else Differential(args))
    # Observe number and order of foreign calls through a real companion TU.
    companion = t.work / 'observe.c'
    companion.write_text('''#include <stdint.h>
static volatile int64_t observations;
int64_t observe(int64_t n) { observations = 10 * observations + n; return n; }
int64_t trace(void) { return observations; }
''', encoding='utf-8')
    t.cflags = [companion, *args.cflag]
    source = t.source('effects', '''record Cell { value:i64 }
extern fn observe(n:i64) -> i64;
extern fn trace() -> i64;
extern fn on1x_unique_live_allocations() -> i64;
fn visit() -> i64 {
    let owner = box(record<Cell>{value:42});
    let a = array[10,20,30];
    println(on1x_unique_live_allocations());
    println(a[0]); # elide index
    return a[observe(1):observe(2)][]; # keep slice
}
fn main() {
    println(visit());
    println(on1x_unique_live_allocations());
    println(trace());
}
''')
    emit = [str(t.compiler), '--emit-c', str(source)]
    assert subprocess.check_output(emit) == subprocess.check_output([*emit, *OFF])
    t.compare(source, stdout='1\n10\n1\n0\n12\n')
    if args.cephyr_library:
        generated = t.emit(source, *OFF)
        broken = 'int broken(void) { return (1 + ); }\n'
        code = generated + broken
        failure = t.compile('#line 1 "effects.generated.c"\n' + code, '--no-factir')
        assert failure.status.name == 'COMPILE_ERROR' and not failure.output, failure
        error, = failure.sarif()['runs'][0]['results']
        location = error['locations'][0]['physicalLocation']
        assert error['level'] == 'error' and location['artifactLocation']['uri'] == 'effects.generated.c', error
        assert location['region']['startLine'] == len(code.splitlines()), error
        assert location['region']['startColumn'] == broken.index(');') + 1, error
    if args.cephyr_library or args.driver:
        t.trap('overflow', '    println(a[9223372036854775807 + 1]); # keep index',
               'addition overflow', kind='arithmetic')
        t.trap('division', '    println(a[1 / (2 - 2)]); # keep index',
               'invalid division', kind='arithmetic')
        t.trap('dynamic', '    println(a[observe(3)]); # keep index',
               'index out of bounds', helper='extern fn observe(n:i64) -> i64;\n')
    conflict = t.source('borrow_conflict', '''record Cell { value:i64 }
fn main() {
    let a=array[1,2]; println(a[0]);
    let owner=box(record<Cell>{value:42}); let view=borrow owner;
    drop(owner); println(view["value"]);
}
''')
    off, on = (t.reject('--emit-c', conflict, mode, 1, 'cannot consume') for mode in (OFF, ON))
    assert off == on and f'{conflict}:5:' in off, off
    if args.driver:
        obj = t.work / 'retained.o'
        obj.write_bytes(b'prior artifact')
        rejected = run(t.compiler, '--emit-cephyr-object', conflict, '-o', obj,
                       '--cephyr-factir', 'on', '--cephyr-profile', 'scalar')
        assert rejected.stderr == off and rejected.returncode == 1 and obj.read_bytes() == b'prior artifact'
    print('MEMGraph reference: identical default bytes, foreign effects, cleanup and loan diagnostics passed')


if __name__ == '__main__':
    main()
