#!/usr/bin/env python3
"""On1xMETA source semantics against C11, MIR evaluation and real Archon."""
import json
import hashlib
import random
from support import NativeTests, run


t = NativeTests()
fixtures = t.root / 'processor/tests/fixtures'
archon = t.compiler.parent / 'backends/mir-archon/on1x-archon-mir'
scalar_abi = archon.parent / 'libon1x_archon_scalar_abi.a'
native = archon.is_file() and scalar_abi.is_file()
outcomes = []


def command(argv):
    p = run(argv)
    assert p.returncode == 0, (list(map(str, argv)), p.returncode, p.stderr)
    return p


runtime = t.work / 'runtime.o'
command([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
         '-I', t.root / 'runtime', '-c', t.root / 'runtime/on1x_runtime.c',
         *t.flags, '-o', runtime])


def check_result(name, profile, p, expected, trap):
    assert p.stdout == expected, (name, profile, p.stdout, expected, p.stderr)
    assert p.returncode == (70 if trap else 0), (name, profile, p.returncode, p.stderr)
    if trap:
        assert trap in p.stderr, (name, profile, trap, p.stderr)
    else:
        assert not p.stderr, (name, profile, p.stderr)
    outcomes.append({'case': name, 'profile': profile, 'exit': p.returncode,
                     'stdout': p.stdout})


def exercise(name, source, expected, *, scalar=True, trap=None, extra=()):
    """Compare observable order/status to an independent expected trace."""
    results = []
    modes = [('--emit-c', [])]
    if scalar:
        modes += [('--emit-ir-c', []), ('--emit-ir-c', ['--optimize-ir'])]
    for index, (mode, options) in enumerate(modes):
        profile = mode + (' optimized' if options else '')
        code, exe = t.work / f'{name}-{index}.c', t.work / f'{name}-{index}'
        command([t.compiler, mode, *options, source, '-o', code])
        # Generated output must be stable even after multiple compiler invocations.
        again = command([t.compiler, mode, *options, source])
        assert again.stdout == code.read_text(), (name, profile, 'nondeterminism')
        command([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                 '-I', t.root / 'runtime', code,
                 *([t.root / 'runtime/on1x_runtime.c'] if extra else [runtime]),
                 *extra, *t.flags, '-lm', '-o', exe])
        result = run([exe])
        check_result(name, profile, result, expected, trap)
        results.append(result)
    if scalar:
        for options in [[], ['--optimize-ir']]:
            result = run([t.compiler, '--run-ir', *options, source])
            check_result(name, 'evaluator' + str(options), result, expected, trap)
        if native:
            for options, suffix in [([], 'native'), (['--optimize-ir'], 'native-opt')]:
                ir, asm = t.work / f'{name}-{suffix}.mir', t.work / f'{name}-{suffix}.s'
                exe = t.work / f'{name}-{suffix}'
                command([t.compiler, '--emit-ir', *options, source, '-o', ir])
                command([archon, '--ir-input', ir, '-o', asm])
                command([t.cc, asm, scalar_abi, runtime, *t.flags, '-lm', '-o', exe])
                result = run([exe])
                check_result(name, suffix, result, expected, trap)
                results.append(result)
    # Runtime backends share the same terminal trap contract; the evaluator
    # additionally prefixes a located compiler diagnostic, so compare it above.
    assert all((p.returncode, p.stderr) == (results[0].returncode, results[0].stderr)
               for p in results), (name, [p.stderr for p in results])


exercise('order', fixtures / 'order.on1x',
         '1\ntrue\n2\nfalse\n3\ntrue\n4\nfalse\n5\ntrue\n6\n7\nfalse\n'
         'false\ntrue\n8\nfalse\n9\ntrue\n')
exercise('control', fixtures / 'control.on1x', '10\n11\n13\n3\n')
exercise('hygiene', fixtures / 'hygiene.on1x', '1\ntrue\ntrue\nfalse\n', scalar=False)
hooks = ['-include', t.root / 'tests/fixtures/closure_allocator.h',
         t.root / 'tests/fixtures/closure_allocator.c']
exercise('cleanup', fixtures / 'cleanup.on1x', 'true\n42\n', scalar=False, extra=hooks)
owner_hooks = ['-include', t.root / 'tests/fixtures/unique_allocator.h',
               t.root / 'tests/fixtures/unique_allocator.c']
exercise('owners', fixtures / 'owners.on1x', 'true\nfalse\n42\n',
         scalar=False, extra=owner_hooks)

# Type guards must not generalize boolean/integer identities to floating point.
# +0 is observably different from identity for -0 under IEEE round-to-nearest.
numeric = t.source('numeric_guards', '''
fn addzero(x:f64)->f64 { return x+0.0; }
fn main() {
 println(f64_signbit(addzero(-0.0)));
 println(f64_is_nan(0.0*(0.0/0.0)));
 println(f32_signbit(-0.0f32));
 println(255u8-0u8);
 let nan=0.0/0.0;
 println(nan<=0.0);println(nan>=0.0);println(nan!=nan);
}
''')
exercise('numeric_guards', numeric, 'false\ntrue\ntrue\n255\nfalse\nfalse\ntrue\n')
integer = t.source('integer_order', '''
fn mark(label:i64,value:i64)->i64{println(label);return value;}
fn main(){
 println(mark(1,42)+0);
 println(0+mark(2,-42));
 println(mark(3,-9223372036854775808)-0);
 println(mark(4,9223372036854775807)*1);
 println(1*mark(5,-9223372036854775808));
 println(mark(6,-9223372036854775808)/1);
 println(mark(7,1)!=mark(8,2));
 println(mark(9,2)<=mark(10,1));
 println(mark(11,2)>=mark(12,1));
}
''')
exercise('integer_order', integer,
         '1\n42\n2\n-42\n3\n-9223372036854775808\n4\n9223372036854775807\n'
         '5\n-9223372036854775808\n6\n-9223372036854775808\n'
         '7\n8\ntrue\n9\n10\nfalse\n11\n12\ntrue\n')

# The report is produced by the same checked pipeline used by emitters. Check
# authored rule activation and exact rule provenance, not only file hashes.
report_source = t.source('report', '''
fn boolean(x:bool,y:bool)->bool{return !(!(x&&y))||false;}
fn integer(x:i64)->i64{return (x+0)*1;}
fn main(){println(boolean(true,false));println(integer(42));}
''')
first = command([t.compiler, '--dump-processor', report_source])
second = command([t.compiler, '--dump-processor', report_source])
assert first.stdout == second.stdout and not first.stderr and not second.stderr
report = json.loads(first.stdout)
assert report['format'] == 'on1x.processor' and report['version'] == 1
assert report['verified'] is True
manifest = json.loads((t.compiler.parent / 'processor/generated/rules_manifest.json').read_text())
for identity in ['program_sha256', 'parser_sha256']:
    assert report[identity] == manifest[identity] and len(report[identity]) == 64
for source in manifest['sources']:
    authored = t.root / 'processor' / source['file']
    assert hashlib.sha256(authored.read_bytes()).hexdigest() == source['sha256'], source
for phase in ['structural', 'typed_before', 'typed_after', 'desugar', 'canonicalize']:
    evidence = report[phase]
    assert evidence['visited'] > 0, (phase, evidence)
    names = [rule['name'] for rule in evidence['rules']]
    assert len(names) == len(set(names)), (phase, names)
    for rule in evidence['rules']:
        authored = t.root / 'processor' / rule['file']
        line = authored.read_text().splitlines()[rule['line']-1]
        assert line.startswith(('rule ' + rule['name'] + ' ',
                                'constraint ' + rule['name'] + ' ')), (phase, rule, line)
        assert rule['column'] == 1 and rule['count'] >= 0
    if phase in ['desugar', 'canonicalize']:
        assert evidence['rewrites'] == sum(rule['count'] for rule in evidence['rules'])
        assert evidence['rewrites'] > 0, (phase, evidence)
    else:
        assert evidence['checks'] > 0, (phase, 'dormant semantic phase', evidence)
        assert evidence['checks'] == sum(rule['count'] for rule in evidence['rules'])
        semantic_hits = {rule['name']: rule['count'] for rule in evidence['rules']}
        for name in ['variable_arity', 'call_callee', 'boolean_arity']:
            assert semantic_hits.get(name, 0) > 0, (phase, name, evidence)
        if phase != 'structural':
            for name in ['variable_resolved', 'boolean_type']:
                assert semantic_hits.get(name, 0) > 0, (phase, name, evidence)
hits = {rule['name']: rule['count'] for phase in ['desugar', 'canonicalize']
        for rule in report[phase]['rules']}
for name in ['lazy_and', 'lazy_or', 'boolean_double_not', 'integer_add_zero_right',
             'integer_multiply_one_right']:
    assert hits[name] > 0, (name, hits)
report_file = t.work / 'processor-report.json'
command([t.compiler, '--dump-processor', report_source, '-o', report_file])
assert report_file.read_text() == first.stdout
independent = []
for option in ['--memory-check', '--verify-types']:
    checked = run([t.compiler, '--dump-processor', option, report_source])
    if checked.returncode == 2 and 'unavailable' in checked.stderr:
        print(option + ': UNSUPPORTED (optional SDK not configured)')
    else:
        assert checked.returncode == 0 and checked.stdout == first.stdout, (option, checked.stderr)
        independent.append(option)
if '--memory-check' in independent:
    dead_external = t.source('dead_external',
        'extern fn opaque()->bool;fn main(){println(false&&opaque());}')
    failed_report = t.work / 'failed-report.json'
    failed_report.write_text('preserved\n')
    checked = run([t.compiler, '--dump-processor', '--memory-check', dead_external, '-o', failed_report])
    assert checked.returncode == 1 and 'raw external' in checked.stderr, checked.stderr
    assert not checked.stdout and failed_report.read_text() == 'preserved\n'

# Nested identities and arbitrary boolean inputs use a separate Python oracle.
rng = random.Random(0x01A)
statements, expected = [], []
for _ in range(64):
    a, b = bool(rng.randrange(2)), bool(rng.randrange(2))
    left, right = str(a).lower(), str(b).lower()
    statements.append(f'println(!(!(({left}&&true)||(false||{right}))));')
    expected.append(str(a or b).lower())
# Keep each source inside the native target's documented block-table budget.
for start in range(0, len(statements), 8):
    name = f'boolean_oracle_{start}'
    exercise(name, t.source(name, 'fn main(){' + ''.join(statements[start:start+8]) + '}'),
             '\n'.join(expected[start:start+8]) + '\n')

# Never erase a checked trap, even under a constant-looking annihilator.
for index, (expression, diagnostic) in enumerate([
    ('(1/0==0)&&true', 'invalid division'),
    ('false||(1%0==0)', 'invalid modulo'),
    ('!(!(9223372036854775807+1==0))', 'overflow'),
    ('0*(1/0)', 'invalid division'),
    ('(1/0)*0', 'invalid division'),
    ('(1/0)%1', 'invalid division'),
]):
    source = t.source(f'trap_{index}',
                      f'fn main(){{println(7);println({expression});println(9);}}')
    exercise(f'trap_{index}', source, '7\n', trap=diagnostic)

# Every original source subtree is checked before rewrites may remove it.
# Reject through each public source pipeline and preserve pre-existing output.
negative = [
    ('dead_type', 'fn main(){println(false && (1+true==0));}', 'expected i64'),
    ('dead_name', 'fn main(){println(true || missing);}', 'undefined'),
    ('dead_call', 'fn one(x:bool)->bool{return x;}fn main(){println(false && one());}', 'argument'),
    ('dead_branch', 'fn main(){if false{println(missing);}}', 'undefined'),
    ('dead_capture', 'fn main(){var x=1;if false{let f=fn()->i64{return x;};}}', 'capturing mutable'),
    ('dead_control', 'fn main(){if false{break;}}', 'no lexical target'),
    ('dead_move', 'record Cell{n:i64}fn main(){let b=box(record<Cell>{n:1});'
                  'if false{drop(b);drop(b);}}', 'move'),
]
preserved = t.work / 'preserved-output'
for name, text, diagnostic in negative:
    source = t.source(name, text)
    for mode in ['--check', '--emit-c', '--emit-ir', '--emit-ir-c', '--run-ir', '--dump-processor']:
        preserved.write_text('preserved\n')
        p = run([t.compiler, mode, source, '-o', preserved])
        assert p.returncode == 1 and diagnostic in p.stderr, (name, mode, p.returncode, p.stderr)
        assert not p.stdout and preserved.read_text() == 'preserved\n', (name, mode)
        assert str(source) in p.stderr, (name, mode, 'lost source identity', p.stderr)

for incompatible in [['--ir-input'], ['--optimize-ir']]:
    preserved.write_text('preserved\n')
    p = run([t.compiler, '--dump-processor', *incompatible, report_source, '-o', preserved])
    assert p.returncode == 2 and not p.stdout and preserved.read_text() == 'preserved\n', p.stderr

# Imported macro expansion keeps its call-site diagnostic rather than .o1m paths.
library = t.source('located_macro', 'macro bad{rule()=>expression{false || unknown};}')
source = t.source('located_import', 'import "located_macro.on1x";\nfn main(){println(bad!());}\n')
p = run([t.compiler, '--emit-c', source])
assert p.returncode == 1 and 'undefined' in p.stderr and not p.stdout, p.stderr
assert 'located_' in p.stderr and '.o1m' not in p.stderr, p.stderr

(t.work / 'processor-results.json').write_text(json.dumps(outcomes, indent=2) + '\n')
print(f'On1xMETA semantics PASS: {len(outcomes)} executions, '
      f'{len(negative)*6+3+int("--memory-check" in independent)} rejections')
print('Archon native: ' + ('executed' if native else 'UNSUPPORTED (native tool/ABI not configured)'))
