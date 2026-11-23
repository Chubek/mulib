#!/usr/bin/env python3
"""Differential checked-HIR emission, including the generated ActionSemantics oracle."""
import argparse
import json
from pathlib import Path
import random

from support import run

cli = argparse.ArgumentParser()
for arg in ('compiler', 'root', 'work'):
    cli.add_argument(arg, type=Path)
cli.add_argument('cc')
cli.add_argument('oracle_generator', type=Path)
cli.add_argument('--cflag', action='append', default=[])
cli.add_argument('--expect-smt', action='store_true')
args = cli.parse_args()
compiler, root, work, cc, oracle_generator = (args.compiler, args.root, args.work, args.cc,
                                             args.oracle_generator)
work.mkdir(parents=True, exist_ok=True)
off = ['--profile', 'debug']
on = [*off, '--enable-opt', 'novostone-hir']
common = ['-std=c11', '-O0', *args.cflag]


def ok(*args, **kwargs):
    result = run(args, **kwargs)
    assert result.returncode == 0, (args, result)
    return result.stdout


def source(name, text):
    path = work / f'{name}.on1x'
    path.write_text(text, encoding='utf-8')
    return path


def report(path, flags):
    return json.loads(ok(compiler, '--dump-processor', *flags, path))


def native(path, flags, label):
    generated, exe = work / f'{label}.c', work / label
    ok(compiler, '--emit-c', *flags, path, '-o', generated)
    ok(cc, *common, '-Wall', '-Wextra', '-Wpedantic', '-Werror',
       '-I', root/'runtime', generated, root/'runtime/on1x_runtime.c', '-lm', '-o', exe)
    output = run([exe])
    return output.returncode, output.stdout, output.stderr


oracle_c, oracle = work/'oracle.c', work/'oracle'
oracle_c.write_text(ok(oracle_generator), encoding='utf-8')
ok(cc, *common, oracle_c, '-o', oracle)
terms = [
    ('2*3-5', 'Sub(Mul(Num(2),Num(3)),Num(5))', False),
    ('-(6-2)*8', 'Mul(Sub(Num(0),Sub(Num(6),Num(2))),Num(8))', False),
    ('(0-2147483647)-1+1', 'Add(Sub(Sub(Num(0),Num(2147483647)),Num(1)),Num(1))', False),
    ('(2-9)==-7', 'Eq(Sub(Num(2),Num(9)),Num(-7))', True),
    ('!(4*7==28)', 'Eq(Eq(Mul(Num(4),Num(7)),Num(28)),Num(0))', True),
    ('true!=false', 'Eq(Eq(Num(1),Num(0)),Num(0))', True),
]
rng = random.Random(728)
for _ in range(12):
    a, b, c = [rng.randrange(-30, 31) for _ in range(3)]
    terms.append((f'(({a})*({b}))-({c})', f'Sub(Mul(Num({a}),Num({b})),Num({c}))', False))
answers = []
for _, term, boolean in terms:
    value = int(ok(oracle, input=term).removeprefix('completed '))
    answers.append(str(bool(value)).lower() if boolean else str(value))
arithmetic = source('checked', 'fn main() {' + ''.join(f'println({e});' for e, _, _ in terms) + '}')
expected = '\n'.join(answers) + '\n'
assert 'novostone_hir' not in report(arithmetic, off)
r = report(arithmetic, on)
assert r['verified'] and r == report(arithmetic, on)
stats = r['novostone_hir']
assert stats['enabled'] and not stats['limited'] and stats['applied'] == len(terms), stats
assert stats['attempted'] == stats['specialized'] == stats['applied'], stats
assert stats['obligations'] >= len(terms) and 0 <= stats['smt_proved'] <= stats['obligations'], stats
if args.expect_smt:
    assert stats['smt_proved'] > 0, stats
for label, flags in [('reference', off), ('optimized', on)]:
    assert native(arithmetic, flags, label) == (0, expected, '')
    assert ok(compiler, '--run-ir', *flags, arithmetic) == expected
reference = ok(compiler, '--emit-c', *off, arithmetic)
optimized = ok(compiler, '--emit-c', *on, arithmetic)
assert 'on1x_mul_i64(' in reference and 'on1x_mul_i64(' not in optimized

# Wrapping i32 would get both of these wrong. i64 still succeeds, with no
# Novostone rewrite. ActionSemantics has checked i32 and should also decline.
large = source('large', 'fn main(){println(2147483647+1);println((2147483647+1)-1);}')
assert run([oracle], input='Add(Num(2147483647),Num(1))').returncode == 1
assert report(large, on)['novostone_hir']['applied'] == 0
assert ok(compiler, '--emit-c', *off, large) == ok(compiler, '--emit-c', *on, large)
assert native(large, on, 'large') == (0, '2147483648\n2147483647\n', '')

# Compare the whole diagnostic, including location, and the ordered effects.
for i, (expr, diagnostic) in enumerate([
    ('9223372036854775807+(2*3-5)', 'addition overflow'),
    ('-(-9223372036854775808)', 'negation overflow'),
    ('7/(2-2)', 'invalid division'),
    ('-9223372036854775808 % (-1)', 'invalid modulo'),
]):
    trap = source(f'trap{i}', f'fn main(){{println(7);println({expr});println(99);}}')
    plain = native(trap, off, f'trap{i}-off')
    folded = native(trap, on, f'trap{i}-on')
    assert plain == folded and plain[:2] == (70, '7\n') and diagnostic in plain[2], (plain, folded)

effects = source('effects', '''
fn take(x: i64) -> i64 {println(x);return x;}
fn main(){defer println(99);println(take(2*3)+take(7-2));}
''')
assert native(effects, off, 'effects-off') == native(effects, on, 'effects-on') == (0, '6\n5\n11\n99\n', '')
bad = source('bad', 'record Cell{n:i64} fn main(){let b=box(record<Cell>{n:1});drop(b);drop(b);}')
assert run([compiler, '--emit-c', *on, bad]).returncode == 1

names = {x['name'] for x in json.loads(ok(compiler, '--list-optimizations'))['optimizations']}
if 'memgraph-bounds' in names:
    mem = ['--enable-opt', 'memgraph-bounds']
    # The reference MEMGraph caps its local depth; HIR folding can expose a
    # literal, but MEMGraph must still prove the layout and exact range itself.
    index = '+'.join(['1'] * 18) + '-17'
    bounds = source('bounds', f'''
fn do_index(i:i64) -> i64 {{ let a=array[11,22,33]; return a[i]; }}
fn main(){{let a=array[11,22,33];println(a[{index}]);println(do_index(2));}}
''')
    before = json.loads(ok(compiler, '--dump-memgraph', *off, *mem, bounds))
    after = json.loads(ok(compiler, '--dump-memgraph', *on, *mem, bounds))
    assert (before['applied'], before['retained']) == (0, 2), before
    assert (after['applied'], after['retained']) == (1, 1), after
    assert native(bounds, [*off, *mem], 'bounds-off') == native(bounds, [*on, *mem], 'bounds-on') == (0, '22\n33\n', '')

# Once the unit cap is reached further sites remain ordinary checked HIR.
many = source('budget', 'fn main(){' + 'println(2+3);' * 70 + '}')
capped = report(many, on)['novostone_hir']
assert capped['limited'] and capped['attempted'] == capped['applied'] == 64, capped
assert ok(compiler, '--run-ir', *on, many) == '5\n' * 70
