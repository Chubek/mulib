#!/usr/bin/env python3
"""Optimization policy must reach the frontend, lowering and actual MIR pipeline."""
import json
import sys
from support import NativeTests, run

t = NativeTests()
sys.path.insert(0, str(t.root/'stdlib'))
import sexpr


def ok(*args):
    p = run([t.compiler, *args])
    assert p.returncode == 0, (args, p.stderr)
    return p.stdout


def listing(*args):
    rows = json.loads(ok('--list-optimizations', *args))['optimizations']
    assert len({r['name'] for r in rows}) == len(rows)
    return {r['name']: (r['stage'], r['enabled']) for r in rows}


debug = ['--opt-profile', 'debug']
release = ['--profile', 'release']
baseline = listing()
assert baseline == listing('--opt-profile', 'default')
assert all(on == (stage not in ('mir', 'c11') and name != 'partial-eval')
           for name, (stage, on) in baseline.items())
assert {'decision-ssa', 'integer_add_zero_right', 'constant-fold', 'dead-pure'} <= baseline.keys()
assert not any(on for _, on in listing(*debug).values())
assert all(on for _, on in listing(*release).values())
assert listing('--all-optimizations') == listing(*release)
assert listing('--no-optimizations') == listing(*debug)
one = ['--enable-opt', 'integer_add_zero_right']
only = listing(*one, '--disable-opt', 'all', *release)
assert only == listing(*release, '--disable-opt', 'all', *one)
assert [name for name, (_, on) in only.items() if on] == [one[1]]

source = t.source('identity', '''
fn plus(x:i64)->i64{return (x+0)*1;}
fn mark(n:i64,b:bool)->bool{println(n);return b;}
fn pick(a:bool,b:bool)->bool{return mark(1,a)&&mark(2,b);}
fn main(){println(plus(42));println(pick(false,true));println(pick(true,false));}
''')
expected = '42\n1\nfalse\n1\n2\nfalse\n'
for opts in [[], debug, release, [*debug, *one]]:
    report = json.loads(ok('--dump-processor', source, *opts))
    assert report['verified']
    assert all(report[p]['checks'] for p in ['structural', 'typed_before', 'typed_after'])
    hits = {r['name'] for p in ['canonicalize', 'desugar'] for r in report[p]['rules'] if r['count']}
    if opts == debug:
        assert not hits
    elif opts == debug + one:
        assert hits == {one[1]}, hits
    else:
        assert {one[1], 'integer_multiply_one_right', 'lazy_and'} <= hits
    generated = ok('--emit-c', source, *opts)
    assert ('on1x_add_i64(' in generated) == (opts == debug)
    assert ('on1x_mul_i64(' in generated) == (opts[:2] == debug)
    assert ok('--run-ir', source, *opts) == expected

signature = source.with_suffix('.on1xi')
signature.write_text('on1xi 1;\nfn plus(x:i64)->i64;\n')
assert 'OK:' in ok('--check-interfaces', source, '--interface', signature, *debug, *one)

# decision-ssa is independently switchable from generic lazy desugaring.
decision = t.source('decision', 'fn pick(a:bool,b:bool)->bool{return a&&b;}'
                    'fn main(){println(pick(true,false));}')
ssa = ok('--emit-ir', decision)
slots = ok('--emit-ir', decision, '--disable-opt', 'decision-ssa')
assert ssa != slots and ' store ' not in ssa and ' store ' in slots, (ssa, slots)

fixture = t.root/'examples/optimize.on1x'
plain = ok('--emit-ir', fixture)
legacy = ok('--emit-ir', fixture, '--optimize-ir')
assert plain != legacy == ok('--emit-ir', fixture, *release, '--disable-opt', 'partial-eval')
assert plain == ok('--emit-ir', fixture, '--opt-profile', 'default')
saved = t.work/'plain.mir'
saved.write_text(plain)
assert legacy == ok('--ir-input', '--emit-ir', saved, '--enable-opt', 'all')
assert plain == ok('--ir-input', '--emit-ir', saved, '--optimize-ir', '--no-optimizations')
assert ok('--ir-input', '--run-ir', saved, *release) == '1\n43\n42\n'
default_dump = sexpr.loads(ok('--dump-passes', '--ir-input', saved))
assert [r[1] for r in default_dump[8]] == [n for n, (s, _) in baseline.items() if s == 'mir']
for flags, names in [(debug, []), (debug + ['--enable-opt', 'constant-fold'], ['constant-fold']),
                     (['--optimize-ir', '--no-optimizations'], [])]:
    tree = sexpr.loads(ok('--dump-passes', '--ir-input', saved, *flags))
    assert tree[6][1] == 'complete' and tree[3] == ['source', plain]
    assert [r[1] for r in tree[8]] == names
    assert {e[1] for e in tree[9]} == set(names)
    if not names:
        assert tree[7] == ['consumed', '0', '0'] and tree[4] == ['output', plain]
    else:
        assert tree[4][1] != plain and any(e[5] == 'committed' for e in tree[9])

# The target-aware native SDK consumes the same transformed verified module.
native = t.compiler.parent/'backends/mir-archon/on1x-archon-mir'
if native.is_file() and '--sdk-target' in ok('--help'):
    options = ['--sdk-emit', 'mir-archon', '--sdk-target', 'x86_64-unknown-linux-gnu']
    asm = ok(*options, fixture, *release, '--disable-opt', 'partial-eval')
    assert asm == ok(*options, fixture, '--optimize-ir')
    assert asm == ok(*options, '--ir-input', saved, *release)
    assert asm != ok(*options, fixture)

out = t.work/'preserved'
def reject(args, text, code=2):
    out.write_text('keep\n')
    p = run([t.compiler, '-o', out, *args])
    assert (p.returncode, p.stdout, out.read_text()) == (code, '', 'keep\n'), (args, p)
    assert text in p.stderr, (args, p.stderr)


for args, diagnostic in [
    (['--list-optimizations', '--disable-opt', 'does-not-exist'], 'unknown optimization'),
    (['--list-optimizations', '--enable-opt'], 'requires a name'),
    (['--emit-c', source, '--opt-profile', '--enable-opt'], 'requires a name'),
    (['--list-optimizations', '--profile', 'fast'], 'unknown optimization profile'),
    (['--list-optimizations', *debug, *release], 'one optimization profile'),
    (['--all-optimizations', '--disable-opt', 'all'], 'conflicting optimization'),
    (['--check', source, *one, '--disable-opt', one[1]], 'conflicting optimization'),
    (['--list-optimizations', '--emit-c'], 'compilation mode'),
    (['--emit-ir', '--list-optimizations'], 'compilation mode'),
    (['--emit-c', source, '--enable-opt', 'dead-pure'], 'not applicable'),
    (['--ir-input', '--emit-ir', saved, '--disable-opt', 'decision-ssa'], 'not applicable'),
    (['--ir-input', '--emit-ir', saved, *one], 'not applicable'),
    (['--tokens', source, '--disable-opt', 'boolean_arity'], 'unknown optimization'),
]:
    reject(args, diagnostic)
invalid = t.source('invalid', 'fn main(){println(false && missing);}')
reject(['--dump-processor', invalid, *debug], 'undefined', 1)
bad_move = t.source('invalid_move', 'record Cell{n:i64}fn main(){let b=box(record<Cell>{n:1});'
                    'if false{drop(b);drop(b);}}')
trap = t.source('trap', 'fn main(){println(7);println((1/0)+0);println(9);}')
for opts in [debug, release]:
    reject(['--emit-c', bad_move, *opts], 'move', 1)
    p = run([t.compiler, '--run-ir', trap, *opts])
    assert (p.returncode, p.stdout) == (70, '7\n') and 'invalid division' in p.stderr, p
broken = t.work/'invalid.mir'
broken.write_text('(on1x.mir 999 1 ())')
reject(['--dump-passes', '--ir-input', broken, *debug], 'version', 1)
print('Optimization policy: profiles, individual rules, lowering, MIR pipelines, '
      'artifact/trace parity, no-op verification and fail-before-publication passed.')
