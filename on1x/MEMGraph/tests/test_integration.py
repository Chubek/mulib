#!/usr/bin/env python3
"""Differential executable tests for the direct-C11 MEMGraph bounds pass."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import subprocess


OFF = ('--disable-opt', 'memgraph-bounds')
ON = ('--enable-opt', 'memgraph-bounds')
FIXTURE = Path(__file__).with_name('fixed-array.on1x').resolve()
# Check calls, not mentions in comments or declarations. The last arguments
# are the original On1x file and line (plus a column for regex captures).
HELPERS = {
    'on1x_bounds_index': 'index',
    'on1x_bounds_slice': 'slice',
    'on1x_regex_index_v1': 'regex',
}
HELPER_NAME = '|'.join(HELPERS)
CALL = re.compile(rf'^\s*(?:[^;\n]*?=\s*)?({HELPER_NAME})\('
                  r'[^\n]*,\s*"(?:[^"\\]|\\.)*",\s*(\d+)(?:,\s*\d+)?\);', re.M)
ANNOTATION = re.compile(r'\b(elide|keep) (index|slice|regex)\b')


def run(*args):
    return subprocess.run([str(arg) for arg in args], capture_output=True, text=True,
                          encoding='utf-8', timeout=60)


def require_success(result):
    assert result.returncode == 0, (result.args, result.returncode, result.stdout, result.stderr)
    return result


def expected_checks(source, *, enabled):
    checks = Counter()
    for line_number, line in enumerate(source.splitlines(), 1):
        for fate, kind in ANNOTATION.findall(line.partition('#')[2]):
            if fate == 'keep' or not enabled:
                checks[line_number, kind] += 1
    return checks


def check_emission(source, generated, *, enabled):
    calls = CALL.findall(generated)
    assert len(calls) == len(re.findall(rf'\b(?:{HELPER_NAME})\(', generated)), generated
    actual = Counter((int(line), HELPERS[name]) for name, line in calls)
    expected = expected_checks(source.read_text(encoding='utf-8'), enabled=enabled)
    assert actual == expected, (source.name, enabled, actual, expected)


def without_elided_checks(source, generated):
    contents = source.read_text(encoding='utf-8')
    elided = expected_checks(contents, enabled=False) - expected_checks(contents, enabled=True)
    kept = []
    for line in generated.splitlines(keepends=True):
        call = CALL.match(line)
        if call:
            helper, line_number = call.groups()
            key = int(line_number), HELPERS[helper]
            if elided[key] > 0:
                elided[key] -= 1
                continue
        kept.append(line)
    assert not +elided, (source, elided)
    return ''.join(kept)


class Differential:
    def __init__(self, args):
        self.compiler = args.on1xc.resolve()
        self.work = args.work_dir.resolve()
        self.work.mkdir(parents=True, exist_ok=True)
        self.runtime = args.on1x_source_dir.resolve() / 'runtime'
        self.cc = (args.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                   '-I', self.runtime)
        self.cflags = args.cflag

    def source(self, name, contents):
        source = self.work / (name + '.on1x')
        source.write_text(contents, encoding='utf-8')
        return source

    def emit(self, source, *options):
        return require_success(run(self.compiler, '--emit-c', source, *options)).stdout

    def report(self, source, *options):
        return require_success(run(self.compiler, '--dump-memgraph', source, *options)).stdout

    def execute(self, source, mode, generated):
        output = self.work / f'{source.stem}.{mode}.c'
        output.write_text(generated, encoding='utf-8')
        executable = output.with_suffix('')
        require_success(run(*self.cc, output, self.runtime / 'on1x_runtime.c',
                            *self.cflags, '-o', executable))
        result = run(executable)
        return result.returncode, result.stdout, result.stderr

    def compare(self, source, *, status=0, stdout, stderr=''):
        expected = status, stdout, stderr
        emissions = []
        for mode, options in [('off', OFF), ('on', ON)]:
            generated = self.emit(source, *options)
            check_emission(source, generated, enabled=(mode == 'on'))
            emissions.append(generated)
            actual = self.execute(source, mode, generated)
            assert actual == expected, (source.name, mode, actual, expected)
        # Eliding a runtime check must not change any operand, computation,
        # cleanup, or locations on the remaining emitted code.
        assert without_elided_checks(source, emissions[0]) == emissions[1], source

    def trap(self, name, statement, message, *, stdout='11\n', helper='', kind='value',
             trap_at=-1, reason=None):
        # Every failing case also contains an eligible access, so it tests the
        # checked fallback in an actual invocation of the optimization.
        contents = (helper + 'fn main() {\n'
                    '    let a = array[11,22,33];\n'
                    '    println(a[0]); # elide index\n'
                    f'{statement}\n'
                    '    println(999);\n'
                    '}\n')
        source = self.source(name, contents)
        trap_statement = statement.splitlines()[trap_at]
        trap_line = contents.splitlines().index(trap_statement) + 1
        if kind == 'regex':
            column = trap_statement.index('groups[') + 1
            stderr = f'{source}:{trap_line}:{column}: On1x regex error: {message}\n'
        elif kind == 'value':
            stderr = f'{source}:{trap_line}: On1x value error: {message}\n'
        else:
            assert kind == 'arithmetic', kind
            # Existing arithmetic errors have no source position in the runtime.
            stderr = f'On1x arithmetic error: {message}\n'
        self.compare(source, status=73 if kind == 'regex' else 70, stdout=stdout, stderr=stderr)
        if reason is not None:
            retained = [e for e in json.loads(self.report(source))['events']
                        if e['line'] == trap_line and e['status'] == 'retained']
            assert len(retained) == 1 and retained[0]['reason'] == reason, (source, retained)

    def reject(self, mode, source, options, status, diagnostic):
        output = self.work / 'must-not-overwrite'
        output.write_text('already published\n', encoding='utf-8')
        result = run(self.compiler, mode, source, *options, '--color', 'never', '-o', output)
        assert (result.returncode, result.stdout) == (status, ''), result
        assert diagnostic in result.stderr, result
        assert output.read_text(encoding='utf-8') == 'already published\n', result
        return result.stderr


def check_report(t):
    raw = t.report(FIXTURE)
    assert raw == t.report(FIXTURE), 'nondeterministic MEMGraph report'
    plan = json.loads(raw)
    contents = FIXTURE.read_text(encoding='utf-8')
    off, on = (expected_checks(contents, enabled=flag) for flag in (False, True))
    applied = off - on
    assert plan['profile'] == 'c11_fixed_array_v1' and plan['enabled'] is True, plan
    assert plan['exhausted'] is False and plan['truncated'] is False, plan
    assert plan['applied'] == sum(applied.values()), plan
    for status, checks in [('applied', applied), ('retained', on)]:
        expected = Counter()
        for (line, _), count in checks.items():
            expected[line] += count
        assert plan[status] == sum(expected.values()), plan
        assert Counter(e['line'] for e in plan['events'] if e['status'] == status) == expected, plan
    for event in plan['events']:
        assert Path(event['file']) == FIXTURE and event['column'] > 0 and event['reason'], event
        if event['status'] == 'applied':
            assert event['rule'] and Path(event['rule_file']).name == 'c11-fixed-array.mmgi', event
            assert event['rule_line'] > 0, event
    disabled = json.loads(t.report(FIXTURE, *OFF))
    assert disabled['enabled'] is False and disabled['applied'] == 0, disabled


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('on1xc', type=Path)
    parser.add_argument('on1x_source_dir', type=Path)
    parser.add_argument('work_dir', type=Path)
    parser.add_argument('cc')
    parser.add_argument('--cflag', action='append', default=[])
    parser.add_argument('--regex', action='store_true', help='also test Firestone regex capture indexing')
    args = parser.parse_args()
    t = Differential(args)

    check_emission(FIXTURE, t.emit(FIXTURE), enabled=False)
    check_emission(FIXTURE, t.emit(FIXTURE, '--opt-profile', 'debug'), enabled=False)
    # Use a literal-only source here: the release pipeline may otherwise turn
    # some nonliteral source bounds into literal HIR bounds before MEMGraph.
    profile_probe = t.source('profile_probe', 'fn main() { let a = array[11,22,33];\n'
                             '    println(a[0]); # elide index\n'
                             '    println(a[1:3][]); # elide slice\n'
                             '}\n')
    release = ('--opt-profile', 'release')
    check_emission(profile_probe, t.emit(profile_probe, *release), enabled=True)
    check_emission(profile_probe, t.emit(profile_probe, *release, *OFF), enabled=False)
    t.compare(FIXTURE, stdout='11\n33\n3\n0\n33\n1\n101\n22\n102\n1\n'
              '103\n104\n33\n105\n106\n107\n2\n33\n2\n22\n')
    check_report(t)
    # Use a chain of subtractions with zeros (not folded by the default profile).
    nested = t.source('proof_depth', 'fn main() { let a = array[11,22,33];\n'
                      '    println(a[' + ' - '.join(['2'] + ['(1 - 1)'] * 20) + ']); # keep index\n'
                      '}\n')
    t.compare(nested, stdout='33\n')
    unicode_path = t.source('array_é', profile_probe.read_text(encoding='utf-8'))
    unicode_report = json.loads(require_success(run(t.compiler, '--dump-memgraph', unicode_path)).stdout)
    assert all(Path(event['file']) == unicode_path for event in unicode_report['events']), unicode_report

    t.trap('negative_index', '    println(a[-1]); # keep index', 'index out of bounds',
           reason='out-of-range')
    t.trap('index_at_extent', '    println(a[ # keep index\n        3\n    ]);',
           'index out of bounds', trap_at=0, reason='out-of-range')
    for name, bounds in [('negative_slice', '-1:2'), ('reversed_slice', '2:1'),
                         ('end_past_extent', '0:4')]:
        t.trap(name, f'    println(a[{bounds}][]); # keep slice', 'slice bounds out of range')
    t.trap('chained_index', '    println(a[0:2][2]); # elide slice; keep index',
           'index out of bounds', reason='not-a-fixed-array')
    t.trap('chained_slice', '    println(a[0:2][0:3][]); # elide slice; keep slice',
           'slice bounds out of range')
    if args.regex:
        t.trap('regex_capture', '    let groups = capture /(a)/ ~ "a";\n'
               '    println(groups[2]); # keep regex',
               'capture index out of range or no match', kind='regex')
    t.trap('overflowing_index',
           '    println(a[9223372036854775807 + 1 - 9223372036854775807]); # keep index',
           'addition overflow', kind='arithmetic')
    t.trap('overflowing_multiply', '    println(a[9223372036854775807 * 2]); # keep index',
           'multiplication overflow', kind='arithmetic', reason='unproved-bound')
    t.trap('overflowing_subtract', '    println(a[0 - (-9223372036854775808)]); # keep index',
           'subtraction overflow', kind='arithmetic', reason='unproved-bound')
    t.trap('zero_divisor', '    println(a[1 / (2 - 2)]); # keep index',
           'invalid division', kind='arithmetic', reason='unproved-bound')
    t.trap('overflowing_modulo', '    println(a[(-9223372036854775808) % -1]); # keep index',
           'invalid modulo', kind='arithmetic', reason='unproved-bound')

    mark = 'fn mark(tag: i64, value: i64) -> i64 { println(tag); return value; }\n'
    t.trap('effectful_slice', '    println(a[mark(20,-1):mark(21,2)][]); # keep slice',
           'slice bounds out of range', helper=mark, stdout='11\n20\n21\n',
           reason='unproved-bound')
    t.trap('trapping_upper_bound', '    println(a[mark(20,-1):mark(21,0)/0][]); # keep slice',
           'invalid division', helper=mark, stdout='11\n20\n21\n', kind='arithmetic')
    t.trap('trapping_lower_bound', '    println(a[mark(20,0)/0:mark(21,2)][]); # keep slice',
           'invalid division', helper=mark, stdout='11\n20\n', kind='arithmetic')
    t.trap('trapping_base', '    println(make_array()[1]); # elide index',
           'invalid division',
           helper=mark + 'fn make_array() -> array<i64,3> { return array[mark(20,1)/0,22,33]; }\n',
           stdout='11\n20\n', kind='arithmetic')

    ownership = t.source('invalid_ownership', '''
record Cell { n: i64 }
fn main() {
    let a = array[1,2,3];
    println(a[0]);
    let b = box(record<Cell>{n:1});
    if false { drop(b); drop(b); }
}
''')
    move_error = 'use after move of local: b'
    off = t.reject('--emit-c', ownership, OFF, 1, move_error)
    on = t.reject('--emit-c', ownership, ON, 1, move_error)
    reported = t.reject('--dump-memgraph', ownership, (), 1, move_error)
    assert off == on == reported, (off, on, reported)

    # No MIR/VM expansion merely because a C11 pass is available.
    array = t.source('outside_c11', 'fn main() { let a = array[1,2,3]; println(a[0]); }\n')
    for mode in ('--emit-ir', '--emit-ir-c'):
        t.reject(mode, array, (), 1, 'MIR')
        t.reject(mode, array, ON, 2, 'not applicable')
    image = t.work / 'outside_c11.o1vm'
    require_success(run(t.compiler, '--emit-vm', array, '-o', image))
    assert image.read_bytes().startswith(b'O1VM'), image
    t.reject('--emit-vm', array, ON, 2, 'not applicable')

    print('MEMGraph C11: check elision, retained checks, diagnostics and executable parity passed')


if __name__ == '__main__':
    main()
