#!/usr/bin/env python3
"""Checked predicate wrappers must cost exactly as much as explicit equality."""
import json

from support import NativeTests, run


t = NativeTests()
cases = [
    ('boolean_equal_negated_operands', '!L == !R'),
    ('boolean_not_equal_negated_left', '!L != R'),
    ('boolean_not_equal_negated_right', 'L != !R'),
]


def program(explicit, calls):
    source = 'fn flag(label:i64,n:i64)->bool {println(label);return 4/n > 0;}\n'
    for i, (_, pattern) in enumerate(cases):
        expr = 'L == R' if explicit else pattern
        expr = expr.replace('L', 'flag(11,a)').replace('R', 'flag(22,b)')
        source += f'fn eq{i}(a:i64,b:i64)->bool {{return {expr};}}\n'
    return source + 'fn main(){' + ''.join(
        f'println(eq{i}({a},{b}));' for i, a, b in calls) + '}\n'


def emit(mode, source):
    result = run([t.compiler, mode, source])
    assert result.returncode == 0, (mode, result.stderr)
    return result.stdout


calls = [(i, a, b) for i in range(len(cases)) for a in [-1, 1] for b in [-1, 1]]
wrapped = t.source('wrapped', program(False, calls))
explicit = t.source('explicit', program(True, calls))

report = json.loads(emit('--dump-processor', wrapped))
assert report['verified']
hits = {r['name']: r['count'] for r in report['canonicalize']['rules'] if r['count']}
assert hits == {name: 1 for name, _ in cases}, hits

# Equality of complete generated C catches redundant evaluations, temporaries,
# checks and branches without relying on the host compiler's optimization.
for mode in ['--emit-c', '--emit-ir-c']:
    assert emit(mode, wrapped) == emit(mode, explicit), mode

expected = ''.join('11\n22\n' + str(a == b).lower() + '\n' for _, a, b in calls)
t.valid('wrapped', expected, source=wrapped)
assert emit('--run-ir', wrapped) == expected

# A first-operand trap prevents the second call; a second-operand trap must
# still execute after the first call. The trapping division is inside the
# captured expression, rather than in an argument evaluated before eqN.
for i, (name, _) in enumerate(cases):
    for a, b, trace in [(0, 1, '11\n'), (-1, 0, '11\n22\n')]:
        label = f'{name}_{a}_{b}'
        source = t.source(label, program(False, [(i, a, b)]))
        for args in [[t.build(label, source=source)], [t.compiler, '--run-ir', source]]:
            p = run(args)
            assert (p.returncode, p.stdout) == (70, trace), (label, p)
            assert 'invalid division' in p.stderr, (label, p.stderr)

print('Predicate rules: three verified hits, identical direct/MIR C11, '
      '12 truth-table calls and six ordered traps across C11 and MIR evaluation.')

# A closed checked -0 is a different HIR subtree from literal 0. Normalize
# it before add-zero without folding negations of arbitrary captured values.
def signed_zero_source(expr):
    return f'''
fn mark(x:i64)->i64 {{println(7);return x;}}
fn identity(x:i64)->i64 {{return {expr};}}
fn main(){{
  println(identity(-9223372036854775808));
  println(identity(9223372036854775807));
  println(f64_signbit(-0.0));
  println(f32_signbit(-0.0f32));
}}
'''


wrapped = t.source('negated_zero', signed_zero_source('mark(x) + -0'))
explicit = t.source('bare_operand', signed_zero_source('mark(x)'))
for mode in ['--emit-c', '--emit-ir-c']:
    assert emit(mode, wrapped) == emit(mode, explicit), mode
report = json.loads(emit('--dump-processor', wrapped))
hits = {r['name']: r['count'] for r in report['canonicalize']['rules'] if r['count']}
assert hits == {'integer_negate_zero': 1, 'integer_add_zero_right': 1}, hits
expected = '7\n-9223372036854775808\n7\n9223372036854775807\ntrue\ntrue\n'
t.valid('negated_zero', expected, source=wrapped)
assert emit('--run-ir', wrapped) == expected

# The inner -mark(MIN) traps before reaching either the outer minus or
# the addition. Only the independent negated literal and addition may vanish.
source = t.source('negated_min', '''
fn mark(x:i64)->i64 {println(11);return x;}
fn main(){println(-(-mark(-9223372036854775808)) + -0);println(22);}
''')
for args in [[t.build('negated_min', source=source)], [t.compiler, '--run-ir', source]]:
    p = run(args)
    assert (p.returncode, p.stdout) == (70, '11\n'), p
    assert 'negation overflow' in p.stderr, p.stderr
print('Signed i64 -0: exact direct/MIR C11 equality, both integer limits, '
      'IEEE negative zeros and checked double-negation trap preserved.')
