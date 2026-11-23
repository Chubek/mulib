#!/usr/bin/env python3
"""The Firestone pass must change checked HIR, not the runtime observation."""
import json
from support import NativeTests, run

t = NativeTests()
off = ['--profile', 'debug']
on = [*off, '--enable-opt', 'firestone-hir']


def ok(*args):
    result = run(args)
    assert result.returncode == 0, (args, result)
    return result.stdout


def compile_c(source, flags, tag):
    path, exe = t.work/f'{tag}.c', t.work/tag
    ok(t.compiler, '--emit-c', *flags, source, '-o', path)
    ok(t.cc, '-std=c11', '-O0', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
       '-I', t.root/'runtime', path, t.root/'runtime/on1x_runtime.c', '-lm', *t.flags, '-o', exe)
    return path.read_text(), run([exe])


def observe(result):
    return result.returncode, result.stdout, result.stderr


def report(source, flags=on, mode='--dump-processor'):
    return json.loads(ok(t.compiler, mode, *flags, source))


source = t.source('wide', '''
fn mark(x:i64)->i64 { println(7); return x; }
fn main() {
    println(mark(-9223372036854775808) / (3-2));
    println((6000000000 * 2 + 8) / 4);
    println(-17 % 4);
}
''')
expected = '7\n-9223372036854775808\n3000000002\n-1\n'
plain, a = compile_c(source, off, 'wide-off')
folded, b = compile_c(source, on, 'wide-on')
assert observe(a) == observe(b) == (0, expected, '')
assert ok(t.compiler, '--run-ir', *on, source) == expected
assert 'on1x_div_i64(' in plain and 'on1x_div_i64(' not in folded
assert 'on1x_mul_i64(' in plain and 'on1x_mul_i64(' not in folded
assert 'firestone_hir' not in report(source, off)
r = report(source)
assert r == report(source) and r['verified']
assert r['firestone_hir']['folds'] >= 5 and r['firestone_hir']['divide_one'] == 1, r

# On1x intentionally traps on the signed remainder case that Firestone
# succeeds on. Also check that an effectful numerator isn't dropped for % 1.
for i, (expression, diag) in enumerate([
    ('-9223372036854775808 % (-1)', 'invalid modulo'),
    ('-9223372036854775808 / (-1)', 'invalid division'),
    ('7 / (2-2)', 'invalid division'),
    ('9223372036854775807 + 1', 'addition overflow'),
    ('(1/0) / 1', 'invalid division'),
]):
    current = t.source(f'trap-{i}', f'fn main(){{println(7); println({expression});}}')
    _, a = compile_c(current, off, f'trap-{i}-off')
    _, b = compile_c(current, on, f'trap-{i}-on')
    assert observe(a) == observe(b) and b.returncode == 70 and b.stdout == '7\n' and diag in b.stderr
effect = t.source('modulo-one', 'fn mark()->i64{println(7);return 42;} fn main(){println(mark()%1);}')
assert observe(compile_c(effect, on, 'effect-on')[1]) == (0, '7\n0\n', '')

# Firestone exposes the authored add-zero rule on a dynamic operand. It must
# fire on the ordinary processor, not inside the Firestone pass.
handoff = t.source('processor', 'fn add(x:i64)->i64 { return x + (5000000000%2); } fn main(){println(add(42));}')
processor = ['--enable-opt', 'integer_add_zero_right']
hits = lambda flags: {row['name']: row['count'] for row in report(handoff, flags)['canonicalize']['rules']}
assert hits([*off, *processor])['integer_add_zero_right'] == 0
assert hits([*on, *processor])['integer_add_zero_right'] == 1

names = {row['name'] for row in json.loads(ok(t.compiler, '--list-optimizations'))['optimizations']}
if 'novostone-hir' in names:
    both = [*on, '--enable-opt', 'novostone-hir']
    mixed = t.source('mixed', 'fn main(){println(6000000000/4);println(!(6*7==42));}')
    evidence = report(mixed, both)
    assert evidence['firestone_hir']['folds'] == 2 and evidence['novostone_hir']['applied'] == 1, evidence
    assert ok(t.compiler, '--run-ir', *both, mixed) == '1500000000\nfalse\n'
if 'memgraph-bounds' in names:
    # More than MEMGraph's local bound-expression depth; folding supplies only
    # a literal. MEMGraph still checks layout and range and keeps a dynamic site.
    inner = '-'.join(['6000000019'] + ['1'] * 18)
    bounds = t.source('bounds', f'''
fn index(i:i64)->i64 {{ let a=array[11,22,33]; return a[i]; }}
fn main(){{let a=array[11,22,33]; println(a[({inner})%6000000000]); println(index(2));}}
''')
    mem = ['--enable-opt', 'memgraph-bounds']
    before = report(bounds, [*off, *mem], '--dump-memgraph')
    after = report(bounds, [*on, *mem], '--dump-memgraph')
    assert (before['applied'], before['retained']) == (0, 2), before
    assert (after['applied'], after['retained']) == (1, 1), after
    assert observe(compile_c(bounds, [*on, *mem], 'bounds-on')[1]) == (0, '22\n33\n', '')
