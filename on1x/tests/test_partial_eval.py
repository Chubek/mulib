#!/usr/bin/env python3
"""Source partial evaluation against untransformed checked source and direct C11."""
import json
import sys
from support import NativeTests, run

t = NativeTests()
sys.path.insert(0, str(t.root/'stdlib'))
import sexpr

off = ['--profile', 'debug']
on = [*off, '--enable-opt', 'partial-eval']


def ok(*args):
    p = run(args)
    assert p.returncode == 0, (args, p.stdout, p.stderr)
    return p.stdout


def stats(source, flags):
    r = json.loads(ok(t.compiler, '--dump-processor', *flags, source))
    assert r['verified'] and all(r[p]['checks'] for p in ['structural', 'typed_before', 'typed_after'])
    return r['partial_eval']


def operations(source, fn, flags):
    tree = sexpr.loads(ok(t.compiler, '--emit-ir', *flags, source))
    function, = (f for f in tree[3] if f[2] == fn)
    return [i for block in function[6] for i in block[5]]


source = t.source('specialize', '''
fn scale(k:i64,x:i64)->i64 {let factor=k*2+1;return x*factor;}
fn nest(k:i64)->i64{return scale(k+1,3);}
fn mark(x:i64)->i64{println(x);return x;}
fn probe(x:i64)->i64{return scale(4,mark(x));}
fn known()->i64{return nest(4)+7;}
fn main(){println(probe(5));println(known());println(!(4<3)&&(-15%4 == -3));}
''')
assert not stats(source, [])['enabled']
assert not stats(source, off)['enabled']
r = stats(source, on)
assert r == stats(source, on)
assert r['enabled'] and not r['limited'] and r['folds'] >= 3 and r['static_calls'] >= 1 and r['residual_calls'] >= 1, r
assert stats(source, ['--profile', 'release'])['enabled']
assert not stats(source, ['--profile', 'release', '--disable-opt', 'partial-eval'])['enabled']
probe = operations(source, 'probe', on)
assert [i[2] for i in probe].count('call') == 1 and 'mul.checked' in [i[2] for i in probe], probe
assert any(i[2] == 'const' and i[5] == '9' for i in probe), probe
assert [i[2] for i in operations(source, 'probe', off)].count('call') == 2
known = operations(source, 'known', on)
assert len(known) == 1 and known[0][2] == 'const' and known[0][5] == '40', known

# All arguments were evaluated at entry to a strict function, regardless of
# parameter use count or whether its result expression is lazy.
strict = t.source('strict', '''
fn mark(n:i64)->i64{println(n);return n;}
fn flag(n:i64)->bool{println(n);return true;}
fn ignored(k:i64,x:i64)->i64{return k;}
fn repeated(k:i64,x:i64)->i64{return k+x+x;}
fn lazy(k:bool,x:bool)->bool{return k&&x;}
fn unconditional(k:bool,x:bool)->bool{return k||x;}
fn several(k:i64,x:i64,y:i64)->i64{return y-x+k;}
fn noisy(k:i64)->i64{let unused=mark(99);return k;}
fn main(){
 println(ignored(4,mark(1)));println(repeated(3,mark(2)));
 println(lazy(false,flag(3)));println(unconditional(false,flag(4)));
 println(several(2,mark(5),mark(6)));println(noisy(7));
}
''')
assert stats(strict, on)['residual_calls'] == 1


def execute(label, source, expected, trap=None):
    for mode in ['--run-ir', '--emit-c']:
        results = []
        for suffix, opts in [('off', off), ('on', on)]:
            if mode == '--run-ir':
                p = run([t.compiler, mode, *opts, source])
            else:
                path, exe = t.work/f'{label}-{suffix}.c', t.work/f'{label}-{suffix}'
                ok(t.compiler, mode, *opts, source, '-o', path)
                ok(t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                   '-I', t.root/'runtime', path, t.root/'runtime/on1x_runtime.c', *t.flags, '-lm', '-o', exe)
                p = run([exe])
            assert (p.returncode, p.stdout) == (70 if trap else 0, expected), (label, mode, p)
            assert trap in p.stderr if trap else not p.stderr, (label, mode, p.stderr)
            results.append(p.stderr)
        # Includes exact original operator location in evaluator diagnostics.
        assert results[0] == results[1], (label, mode, results)


execute('success', source, '5\n45\n40\ntrue\n')
execute('strict', strict, '1\n4\n2\n7\n3\nfalse\n4\ntrue\n5\n6\n3\n99\n7\n')

# Independent integer oracle: truncating division, signed remainder and
# near-limit arithmetic. Floating arithmetic and unsigned types stay residual.
lo, hi = -(1 << 63), (1 << 63)-1
arithmetic, answers = [], []
for a, b in [(lo, 1), (hi, -1), (lo, 2), (hi, 2), (-17, 4), (17, -4), (0, -1)]:
    q = (abs(a)//abs(b)) * (-1 if (a < 0) != (b < 0) else 1)
    for op, answer in [('/', q), ('%', a-b*q), ('*', a*b), ('+', a+b), ('-', a-b),
                       ('<', a < b), ('>=', a >= b), ('!=', a != b)]:
        if lo <= answer <= hi:
            arithmetic.append(f'println(({a}){op}({b}));')
            answers.append(str(answer).lower())
arithmetic += ['println(f64_signbit(-0.0));', 'println(250u8-1u8);']
answers += ['true', '249']
edge = t.source('numeric', 'fn main(){' + ''.join(arithmetic) + '}')
assert stats(edge, on)['folds'] >= len(answers)-2
assert any(i[2] == 'numeric' for i in operations(edge, 'main', on))
execute('numeric', edge, '\n'.join(answers) + '\n')
for i, (expr, diagnostic) in enumerate([
    ('9223372036854775807+k', 'addition overflow'),
    ('-9223372036854775808-k', 'subtraction overflow'),
    ('9223372036854775807*(k+1)', 'multiplication overflow'),
    ('-(-9223372036854775808)', 'negation overflow'),
    ('1/(k-1)', 'invalid division'),
    ('-9223372036854775808 % (-k)', 'invalid modulo'),
]):
    # The trapping left operand is in the callee, *after* the effectful
    # argument. Discarding/reordering even one dynamic operand changes stdout.
    st = t.source(f'trap_{i}', f'''
fn mark()->i64{{println(17);return 42;}}
fn take(k:i64,x:i64)->i64{{return ({expr})+x;}}
fn main(){{println(take(1,mark()));println(99);}}
''')
    assert stats(st, on)['residual_calls'] == 0
    execute(f'trap_{i}', st, '17\n', diagnostic)

# A dynamic residual operator may trap; retain its callee source location.
st = t.source('residual_trap', '''
fn mark()->i64{println(7);return 9223372036854775807;}
fn twice(k:i64,x:i64)->i64{return x*(k+1);}
fn main(){println(twice(1,mark()));}
''')
assert stats(st, on)['residual_calls'] == 1
execute('residual_trap', st, '7\n', 'multiplication overflow')

# Cycles and over-depth acyclic calls remain executable calls, never compile
# time evaluation of nontermination. Caller/callee order must not bypass fuel.
chain = ''.join(f'fn f{i}(x:i64)->i64{{return f{i+1}(x+1);}}\n' for i in range(12))
deep = t.source('deep', chain + 'fn f12(x:i64)->i64{return x+1;}'
                'fn cycle(x:i64)->i64{return cycle(x);}'
                'fn recursive()->i64{return cycle(1);}'
                'fn drop_arg(k:i64,x:i64)->i64{return k;}'
                'fn divergent()->i64{return drop_arg(4,cycle(1));}'
                'fn main(){println(f0(1));}')
assert stats(deep, on)['limited']
for name in ['recursive', 'main']:
    assert any(i[2] == 'call' for i in operations(deep, name, on)), name
assert sum(i[2] == 'call' for i in operations(deep, 'divergent', on)) == 2
assert ok(t.compiler, '--run-ir', *on, deep) == '14\n'

for i, (text, diagnostic) in enumerate([
    ('fn main(){println(false && missing);}', 'undefined'),
    ('record Cell{n:i64}fn main(){let b=box(record<Cell>{n:1});if false{drop(b);drop(b);}}', 'move'),
]):
    invalid = t.source(f'invalid_{i}', text)
    dest = t.work/'preserved'
    dest.write_text('keep\n')
    p = run([t.compiler, '--dump-processor', *on, invalid, '-o', dest])
    assert p.returncode == 1 and diagnostic in p.stderr and str(invalid) in p.stderr
    assert not p.stdout and dest.read_text() == 'keep\n'
print('Partial evaluation: exact static values and mixed residual MIR, source/strict-C11 '
      'differentials, trap locations/effects, recursion limits and mandatory admission passed.')
