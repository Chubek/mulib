#!/usr/bin/env python3
"""On1xMETA source semantics against C11, MIR evaluation and real Archon."""
import json
import hashlib
import random
from support import NativeTests, run


t = NativeTests()
processor = t.root / 'hir/On1xMETA'
fixtures = processor / 'tests/fixtures'
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
# Macro patterns observe raw syntax before typed identities disappear.
# `helper` in the replacement is definition-site; `$x` is caller-site.
raw = t.source('raw_identity_macro', '''
fn helper(b:bool)->bool {println(1);return !b;}
macro shape {
 rule(ast_binary("&&", $x:Expression, true))=>expression{true && helper($x)};
 rule($x:Expression)=>expression{false};
}
fn main(){
 let helper=fn(b:bool)->bool{println(99);return b;};
 let b=false;println(shape!(b && true));println(shape!(b));
}
''')
exercise('raw_identity_macro', raw, '1\ntrue\nfalse\n', scalar=False)
hooks = ['-include', t.root / 'tests/fixtures/closure_allocator.h',
         t.root / 'tests/fixtures/closure_allocator.c']
exercise('cleanup', fixtures / 'cleanup.on1x', 'true\n42\n', scalar=False, extra=hooks)
owner_hooks = ['-include', t.root / 'tests/fixtures/unique_allocator.h',
               t.root / 'tests/fixtures/unique_allocator.c']
exercise('owners', fixtures / 'owners.on1x', 'true\nfalse\n42\n',
          scalar=False, extra=owner_hooks)
consume = '''
record Cell {n:i64}
fn consume(b:own<Cell>)->bool {
 let value=take(b);println(value["n"]);return value["n"] > 0;
}
'''
owner_calls = t.source('owner_calls', consume + '''
extern fn on1x_test_unique_allocator_live()->i64;
fn main(){
 println(false && consume(box(record<Cell>{n:91})));
 println(true && (!consume(box(record<Cell>{n:7})) != true));
 assert(on1x_test_unique_allocator_live()==0);
}
''')
exercise('owner_calls', owner_calls, 'false\n7\ntrue\n', scalar=False, extra=owner_hooks)

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
 println(mark(13,-9223372036854775808)+ -0);
}
''')
exercise('integer_order', integer,
         '1\n42\n2\n-42\n3\n-9223372036854775808\n4\n9223372036854775807\n'
         '5\n-9223372036854775808\n6\n-9223372036854775808\n'
          '7\n8\ntrue\n9\n10\nfalse\n11\n12\ntrue\n13\n-9223372036854775808\n')

# `!=` has overlapping structural patterns. An i64 guard failure must leave
# both captures intact for the bool rule; a float must match neither guard.
def hits_for(source, phase):
    data = json.loads(command([t.compiler, '--dump-processor', source]).stdout)
    return {r['name']: r['count'] for r in data[phase]['rules']}

numeric_hits = hits_for(numeric, 'desugar')
for name in ['boolean_not_equal', 'integer_not_equal', 'integer_less_equal',
             'integer_greater_equal']:
    assert numeric_hits[name] == 0, (name, numeric_hits)
integer_hits = hits_for(integer, 'desugar')
assert integer_hits['integer_not_equal'] == 1 and integer_hits['boolean_not_equal'] == 0
assert hits_for(integer, 'canonicalize')['integer_negate_zero'] == 1

# Check both truth and eager left-to-right evaluation on *all* inputs.
equal_forms = ['!mark(1,a) == !mark(2,b)', '!mark(1,a) != mark(2,b)',
               'mark(1,a) != !mark(2,b)']
def spelling(value):
    return str(value).lower()

tests = []
expected = []
for a in [False, True]:
    for b in [False, True]:
        tests.append(f'trace({spelling(a)},{spelling(b)});')
        expected.extend(['1', '2', spelling(a == b)] * len(equal_forms))
ordered = t.source('eager_comparison', '''
fn mark(n:i64,x:bool)->bool {println(n);return x;}
fn trace(a:bool,b:bool) {''' + ''.join('println(' + e + ');' for e in equal_forms)
                   + '}\nfn main(){' + ''.join(tests) + '}')
exercise('eager_comparison', ordered, '\n'.join(expected) + '\n')
eager_report = json.loads(command([t.compiler, '--dump-processor', ordered]).stdout)
eager_hits = {r['name']: r['count'] for r in eager_report['canonicalize']['rules']}
ordered_desugar = {r['name']: r['count'] for r in eager_report['desugar']['rules']}
assert ordered_desugar['integer_not_equal'] == 0 and ordered_desugar['boolean_not_equal'] == 2
for name in ['boolean_equal_negated_operands', 'boolean_not_equal_negated_left',
             'boolean_not_equal_negated_right']:
    assert eager_hits.get(name) == 1, (name, eager_hits)

# Byte-for-byte direct and unoptimized MIR-C11 output, including function
# bodies, calls, checked operations and branches. Reuse one path to avoid
# comparing source filenames. Do not erase or renumber instructions/blocks.
def emitted_pair(a, b):
    def emit(body):
        path = t.source('matched_programs', '''
fn mark(n:i64,x:bool)->bool {println(n);return x;}
fn same(a:bool,b:bool)->bool {return ''' + body + ''';}
fn main(){println(same(true,false));}''')
        return [command([t.compiler, mode, path]).stdout for mode in ['--emit-c', '--emit-ir-c']]
    for mode, direct, wrapped in zip(['C11', 'MIR-C11'], emit(a), emit(b)):
        assert direct == wrapped, (mode, a, b, 'nonidentical generated code')

for expr in equal_forms:
    emitted_pair('mark(1,a) == mark(2,b)', 'true && ((' + expr + ') || false)')

# The inner !=, and the combination of lazy and eager negations, reveal the
# neutral RHS only while normalizing this expression. A whole-unit prepass
# followed by irreversible Select expansion misses these opportunities.
for rhs in ['(true != false)', '!(true && !true)', '(true || true)',
            '!(false && false)']:
    emitted_pair('mark(1,a)', 'mark(1,a) && ' + rhs)
closed = t.source('closed_boolean_pairs',
                  'fn main(){println(false && false);println(true || true);}')
closed_hits = hits_for(closed, 'desugar')
assert closed_hits['lazy_and_false_literals'] == closed_hits['lazy_or_true_literals'] == 1

# Pre-normalization exposes `!false` before && can expand to a region;
# post-normalization reduces the `== true` introduced by desugaring `!=`.
before_after = '(mark(1,a) && !false) != true'
emitted_pair('!mark(1,a)', before_after)
phase_pair = json.loads(command([t.compiler, '--dump-processor',
                                 t.work / 'matched_programs.on1x']).stdout)
for phase, names in [('canonicalize', ['boolean_not_false', 'boolean_equal_true_right']),
                     ('desugar', ['lazy_and_true_right', 'boolean_not_equal'])]:
    by_name = {r['name']: r['count'] for r in phase_pair[phase]['rules']}
    for name in names:
        assert by_name.get(name) == 1, (phase, name, by_name)

# The report is produced by the same checked pipeline used by emitters. Check
# authored rule activation and exact rule provenance, not only file hashes.
report_source = t.source('report', '''
fn boolean(x:bool,y:bool)->bool{return !(!(x&&y))||y;}
fn integer(x:i64)->i64{return (x+0)*1;}
fn main(){println(boolean(true,false));println(integer(42));}
''')
first = command([t.compiler, '--dump-processor', report_source])
second = command([t.compiler, '--dump-processor', report_source])
assert first.stdout == second.stdout and not first.stderr and not second.stderr
report = json.loads(first.stdout)
assert report['format'] == 'on1x.processor' and report['version'] == 1
assert report['verified'] is True
manifest = json.loads((t.compiler.parent / 'hir/On1xMETA/generated/rules_manifest.json').read_text())
for identity in ['program_sha256', 'parser_sha256']:
    assert report[identity] == manifest[identity] and len(report[identity]) == 64
for source in manifest['sources']:
    authored = processor / source['file']
    assert hashlib.sha256(authored.read_bytes()).hexdigest() == source['sha256'], source
for phase in ['structural', 'typed_before', 'typed_after', 'desugar', 'canonicalize']:
    evidence = report[phase]
    assert evidence['visited'] > 0, (phase, evidence)
    names = [rule['name'] for rule in evidence['rules']]
    assert len(names) == len(set(names)), (phase, names)
    for rule in evidence['rules']:
        authored = processor / rule['file']
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

# `order` above checks the calls' observable trace. Its compiled rule counts
# distinguish the direct identities from generic Select expansion.
order_report = json.loads(command([t.compiler, '--dump-processor', fixtures / 'order.on1x']).stdout)
order_hits = {r['name']: r['count'] for r in order_report['desugar']['rules'] if r['count']}
assert order_hits == {'lazy_and_true_left': 1, 'lazy_and_true_right': 2,
                      'lazy_or_false_left': 2, 'lazy_or_false_right': 1,
                      'lazy_and': 3, 'lazy_or': 2}, order_hits

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
    # Neither RHS skipping nor eager comparison negation may swallow a
    # checked operation on INT64_MIN; the selected operand itself traps.
    ('false || (!(-(-9223372036854775808)==0) != true)', 'overflow'),
    ('true && (-9223372036854775808/-1==0)', 'invalid division'),
    ('-9223372036854775808%-1', 'invalid modulo'),
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
    ('skipped_owner', consume + 'fn main(){let b=box(record<Cell>{n:1});'
                     'drop(b);println(false && consume(move(b)));}', 'use after move'),
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

# Introduced names point at the definition, spliced names at the caller.
library = t.source('located_macro', 'macro bad{rule()=>expression{false || unknown};}')
source = t.source('located_import', 'import "located_macro.on1x";\nfn main(){println(bad!());}\n')
p = run([t.compiler, '--emit-c', source])
assert p.returncode == 1 and 'undefined' in p.stderr and not p.stdout, p.stderr
assert f'{library}:1:39:' in p.stderr and '.o1m' not in p.stderr, p.stderr
library = t.source('located_macro', 'macro keep{rule($x:Expression)=>expression{true && $x};}')
source = t.source('located_import', 'import "located_macro.on1x";\nfn main(){println(keep!(missing));}\n')
p = run([t.compiler, '--emit-c', source])
assert p.returncode == 1 and 'undefined' in p.stderr and not p.stdout, p.stderr
assert f'{source}:2:25:' in p.stderr and '.o1m' not in p.stderr, p.stderr

(t.work / 'processor-results.json').write_text(json.dumps(outcomes, indent=2) + '\n')
print(f'On1xMETA semantics PASS: {len(outcomes)} executions, '
      f'{len(negative)*6+4+int("--memory-check" in independent)} rejections')
print('Archon native: ' + ('executed' if native else 'UNSUPPORTED (native tool/ABI not configured)'))
