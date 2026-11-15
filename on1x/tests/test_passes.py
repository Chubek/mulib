#!/usr/bin/env python3
"""Independent wide-integer oracle, pipeline artifacts and CLI integration."""
import random
import sys
from support import NativeTests, run

tests = NativeTests()
sys.path.insert(0, str(tests.root/'stdlib'))
import sexpr

def compile_run(path, name):
    exe = tests.work/name
    p = run([tests.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-I', tests.root/'runtime', path, tests.root/'runtime/on1x_runtime.c', *tests.flags, '-o', exe])
    assert p.returncode == 0, p.stderr
    return run([exe])

def check(name, source, expected):
    original = run([tests.compiler, '--emit-ir', source])
    optimized = run([tests.compiler, '--emit-ir', '--optimize-ir', source])
    assert original.returncode == optimized.returncode == 0, (original.stderr, optimized.stderr)
    path = tests.work/(name+'.mir')
    path.write_text(optimized.stdout)
    again = run([tests.compiler, '--emit-ir', '--optimize-ir', '--ir-input', path])
    assert again.returncode == 0 and again.stdout == optimized.stdout and not again.stderr
    for mode, options in [('--emit-c', []), ('--emit-ir-c', []), ('--emit-ir-c', ['--optimize-ir'])]:
        code = tests.work/(name+'.c')
        p = run([tests.compiler, mode, source, *options, '-o', code])
        assert p.returncode == 0, p.stderr
        p = compile_run(code, name)
        assert p.returncode == 0 and p.stdout == expected, (name, p.stdout, expected, p.stderr)
    for options in [[], ['--optimize-ir']]:
        p = run([tests.compiler, '--run-ir', source, *options])
        assert p.returncode == 0 and p.stdout == expected, p.stderr
    p = run([tests.compiler, '--run-ir', '--ir-input', path])
    assert p.returncode == 0 and p.stdout == expected, p.stderr
    return original.stdout, optimized.stdout

source = tests.root/'examples/optimize.on1x'
original, optimized = check('example', source, '1\n43\n42\n')
assert original != optimized and 'div.checked' not in optimized and 'mul.checked' not in optimized
report = run([tests.compiler, '--dump-passes', source])
repeat = run([tests.compiler, '--dump-passes', source])
assert report.returncode == repeat.returncode == 0 and report.stdout == repeat.stdout
(tests.work/'example.passes').write_text(report.stdout)
tree = sexpr.loads(report.stdout)
assert tree[:2] == ['on1x.mir-passes', '1'] and tree[2] == ['target', 'portable-scalar-v1']
assert tree[3] == ['source', original] and tree[4] == ['output', optimized]
assert tree[6][1] == 'complete'
assert [d[1] for d in tree[8]] == ['constant-fold', 'simplify-cfg', 'local-slots', 'dead-pure']
assert any(event[5] == 'committed' for event in tree[9])

check('slots', tests.root/'examples/slots.on1x', '2\n1\n2\n5\n0\n1\n2\n')
# Seeded mutation traces have a separate Python state-machine oracle. Captured
# scalar values must remain snapshots when the original slot is overwritten.
slot_rng = random.Random(907)
for sample in range(8):
    state = [1, 2, 3]
    code = ['var a=1;var b=2;var c=3;']
    trace = []
    for step in range(45):
        dst, src = slot_rng.randrange(3), slot_rng.randrange(3)
        delta = slot_rng.randrange(-10, 11)
        old = state[src]
        code.append(f'let saved{step}={"abc"[src]};')
        code.append(f'{"abc"[dst]}={"abc"[src]}+({delta});')
        state[dst] = old+delta
        code.append(f'println(saved{step});println({"abc"[dst]});')
        trace.extend([str(old), str(state[dst])])
    check(f'slot_trace_{sample}', tests.source(f'slot_trace_{sample}', 'fn main(){'+''.join(code)+'}'), '\n'.join(trace)+'\n')

# Host Python integers provide an oracle independent of shared C++ checked math.
rng = random.Random(8401)
lo, hi = -(1 << 63), (1 << 63)-1
boundary = [lo, lo+1, -3037000500, -1, 0, 1, 3037000499, hi-1, hi]
pairs = [(a, b) for a in boundary for b in boundary]
pairs += [(rng.getrandbits(64)-(1 << 63), rng.getrandbits(64)-(1 << 63)) for _ in range(160)]
statements, expected = [], []
for a, b in pairs:
    for op in ['+', '-', '*', '/', '%', '==', '!=', '<', '<=', '>', '>=']:
        if op in ['/', '%']:
            if b == 0 or (a == lo and b == -1):
                continue
            q = (abs(a)//abs(b)) * (-1 if (a < 0) != (b < 0) else 1)
            value = q if op == '/' else a-q*b
        else:
            value = eval(f'{a}{op}{b}')
        if not lo <= value <= hi:
            continue
        statements.append(f'println(({a}){op}({b}));')
        expected.append(str(value).lower())
# Split to stay within ordinary frontend/MIR verification budgets, not widen them.
for n in range(0, len(statements), 180):
    part = tests.source(f'wide_{n}', 'fn main(){'+''.join(statements[n:n+180])+'}')
    check(f'wide_{n}', part, '\n'.join(expected[n:n+180])+'\n')

# Budget exhaustion is an explicit optimization fallback, not a program trap.
for flag in ['--opt-work', '--opt-rewrites']:
    p = run([tests.compiler, '--emit-ir', '--optimize-ir', source, flag, '0'])
    assert p.returncode == 0 and p.stdout == original and 'last verified MIR prefix' in p.stderr
    p = run([tests.compiler, '--dump-passes', source, flag, '0'])
    assert p.returncode == 0 and sexpr.loads(p.stdout)[6][1] == 'budget-exhausted'
    p = run([tests.compiler, '--run-ir', '--optimize-ir', source, flag, '0'])
    assert p.returncode == 0 and p.stdout == '1\n43\n42\n'
out = tests.work/'preserved'
for options in [['--emit-c', '--optimize-ir'], ['--check', '--optimize-ir'], ['--emit-ir', '--opt-work', '1'],
                ['--dump-passes', '--opt-work', '-1'], ['--dump-passes', '--opt-rewrites', '18446744073709551616']]:
    out.write_text('keep me')
    p = run([tests.compiler, source, *options, '-o', out])
    assert p.returncode == 2 and out.read_text() == 'keep me', p.stderr
unsupported = tests.source('unsupported', 'fn main(){println("text");}')
out.write_text('keep me')
p = run([tests.compiler, '--dump-passes', unsupported, '-o', out])
assert p.returncode == 1 and 'ON1X-MIR-UNSUPPORTED' in p.stderr and out.read_text() == 'keep me'
malformed = tests.work/'malformed.mir'
malformed.write_text('(on1x.mir 999 1 ())')
p = run([tests.compiler, '--dump-passes', '--ir-input', malformed, '-o', out])
assert p.returncode == 1 and 'version' in p.stderr and out.read_text() == 'keep me'

# Independently authored live/dead loop-phi cycles must also execute as strict C11.
phi = tests.work/'phi.c'
p = run([tests.compiler.parent/'on1x_pass_tests', phi])
assert p.returncode == 0, p.stderr
p = compile_run(phi, 'phi')
assert p.returncode == 0 and p.stdout == '20\n10\n', p.stderr

exe = tests.work/'helper'
p = run([sys.executable, tests.root/'tools/on1x-build.py', source, '--via-mir', '--optimize-ir',
         '--on1xc', tests.compiler, '--cc', tests.cc, '-o', exe, *['--cflag='+f for f in tests.flags]])
assert p.returncode == 0, p.stderr
p = run([exe]);assert p.returncode == 0 and p.stdout == '1\n43\n42\n', p.stderr
p = run([sys.executable, tests.root/'tools/on1x-build.py', source, '--optimize-ir', '-o', out])
assert p.returncode == 2 and '--via-mir' in p.stderr and out.read_text() == 'keep me'
print(f'Pass CLI, artifacts, optimized C11, fallback and {len(statements)} independent wide arithmetic cases PASS')
