#!/usr/bin/env python3
"""Three-way execution oracle: reference HIR C11, verified MIR C11, MIR evaluator."""
import hashlib
import random
from support import NativeTests, run

tests = NativeTests()


def native(path, exe):
    p = run([tests.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-I', tests.root/'runtime', path, tests.root/'runtime/on1x_runtime.c',
             *tests.flags, '-o', exe])
    assert p.returncode == 0, p.stderr
    return run([exe])


def compare(name, text=None, source=None, expected=None, status=0, diagnostic=None, options=()):
    source = source or tests.source(name, text)
    outputs = []
    for mode in ['--emit-c', '--emit-ir-c']:
        path = tests.work/(name+mode+'.c')
        p = run([tests.compiler, mode, source, *options, '-o', path])
        assert p.returncode == 0, (name, p.stderr)
        again = run([tests.compiler, mode, source, *options])
        assert again.returncode == 0 and path.read_text() == again.stdout
        p = native(path, tests.work/(name+mode))
        outputs.append((p.returncode, p.stdout))
        assert p.returncode == status, (name, mode, p.returncode, p.stderr)
        if diagnostic:
            assert diagnostic in p.stderr, (name, mode, p.stderr)
    p = run([tests.compiler, '--run-ir', source, *options])
    outputs.append((p.returncode, p.stdout))
    assert outputs[0] == outputs[1] == outputs[2], (name, outputs, p.stderr)
    if expected is not None:
        assert p.stdout == expected, (name, p.stdout, expected)
    if diagnostic:
        assert diagnostic in p.stderr and ': ON1X-MIR-EVAL:' in p.stderr
    a = run([tests.compiler, '--emit-ir', source, *options])
    b = run([tests.compiler, '--emit-ir', source, *options])
    assert a.returncode == b.returncode == 0 and a.stdout == b.stdout
    saved = tests.work/(name+'.mir')
    saved.write_text(a.stdout)
    (tests.work/(name+'.sha256')).write_text(hashlib.sha256(a.stdout.encode()).hexdigest()+'\n')
    reread = run([tests.compiler, '--ir-input', '--emit-ir', saved])
    assert reread.returncode == 0 and reread.stdout == a.stdout, reread.stderr
    reread = run([tests.compiler, '--ir-input', '--run-ir', saved])
    assert (reread.returncode, reread.stdout) == outputs[0], reread.stderr
    reread = run([tests.compiler, '--ir-input', '--emit-ir-c', saved])
    assert reread.returncode == 0 and reread.stdout == (tests.work/(name+'--emit-ir-c.c')).read_text()
    graph = run([tests.compiler, '--ir-cfg', source, *options])
    assert graph.returncode == 0 and graph.stdout.startswith('digraph on1x_mir {')


compare('example', source=tests.root/'examples/mir.on1x', expected='1\n1\n2\n6\n24\n120\nfalse\n')
compare('order', '''
fn mark(n:i64)->i64 {println(n);return n;}
fn pair(a:i64,b:i64)->i64{return a*10+b;}
fn flag(n:i64,v:bool)->bool{println(n);return v;}
fn main(){println(pair(mark(1),mark(2)));println(mark(3)+mark(4));
println(flag(5,false)&&flag(6,true));println(flag(7,true)||flag(8,false));
println(flag(9,true)&&(flag(10,false)||flag(11,true)));}
''', expected='1\n2\n12\n3\n4\n7\n5\nfalse\n7\ntrue\n9\n10\n11\ntrue\n')
compare('scopes', '''
fn main(){var n=1;{let n=42;println(n);}println(n);
while n<5{var k=0;while k<n{k=k+1;}if k==3{println(k);}else{println(-k);}n=n+1;}
var flag=false;while !flag{flag=true;}println(flag);}
''', expected='42\n1\n-1\n-2\n3\n-4\ntrue\n')
compare('returns', '''
fn choose(n:i64)->i64{if n==1{return 10;}else{if n==2{return 20;}}return 30;}
fn find(n:i64)->i64{var i=0;while i<n{if i==3{return i;}i=i+1;}return -1;}
fn nothing(){return;}
fn main()->i64{nothing();println(choose(1));println(choose(2));println(choose(3));println(find(8));return 7;}
''', expected='10\n20\n30\n3\n', status=7)
compare('specialization', '''
fn identity(x)->_{return x;} fn main(){println(identity(42));println(identity(true));}
''', expected='42\ntrue\n')
tests.source('mir_import', 'fn add(x:i64)->i64{return x+1;}')
compare('import', 'import "mir_import.on1x"; fn main(){println(add(41));}', expected='42\n')
compare('macro', '''
macro twice {rule($x:Expression)=>expression{$x+$x};}
fn main(){println(twice!(21));}
''', expected='42\n')
compare('ppx', 'fn main(){println(42);}', expected='42\n',
        options=['--ppx', 'identity', tests.root/'stdppx/identity.py'])
compare('unused', 'fn ignore(x:i64){let y=42;} fn main(){var x=0;x=x;ignore(1);}', expected='')
compare('bounds', '''fn main(){println(-9223372036854775808);println(9223372036854775807);
println(-9223372036854775808*1);println(-9223372036854775808*0);
println(-9223372036854775808+9223372036854775807);println(-7/3);println(-7%3);
println(false&&1/0==0);println(true||1/0==0);println(!false);println(true!=false);}
''', expected='-9223372036854775808\n9223372036854775807\n-9223372036854775808\n0\n-1\n-2\n-1\nfalse\ntrue\ntrue\ntrue\n')

traps = [('9223372036854775807+1', 'addition overflow'),
         ('-9223372036854775808-1', 'subtraction overflow'),
         ('9223372036854775807*2', 'multiplication overflow'),
         ('-9223372036854775808*-1', 'multiplication overflow'),
         ('-(-9223372036854775808)', 'negation overflow'),
         ('1/0', 'invalid division'), ('-9223372036854775808/-1', 'invalid division'),
         ('1%0', 'invalid modulo'), ('-9223372036854775808%-1', 'invalid modulo')]
for n, (expression, diagnostic) in enumerate(traps):
    compare(f'trap_{n}', f'fn main(){{println(19);println({expression});}}',
            status=70, expected='19\n', diagnostic=diagnostic)
compare('assert', 'fn main(){println(9);assert(false);}', status=70,
        expected='9\n', diagnostic='assertion failed')

# Deterministic arithmetic/comparison corpus, independently computed in Python.
rng = random.Random(1201)
statements, expected = [], []
for _ in range(120):
    a, b = rng.randint(-1000000, 1000000), rng.randint(-1000000, 1000000) or 1
    op = rng.choice(['+', '-', '*', '/', '%', '==', '!=', '<', '<=', '>', '>='])
    quotient = (abs(a)//abs(b)) * (-1 if (a < 0) != (b < 0) else 1)
    value = quotient if op == '/' else a-quotient*b if op == '%' else eval(f'{a}{op}{b}')
    statements.append(f'println(({a}){op}({b}));')
    expected.append(str(value).lower())
compare('arithmetic_corpus', 'fn main(){'+''.join(statements)+'}', expected='\n'.join(expected)+'\n')

# Unsupported HIR must preserve pre-existing outputs, including hidden unused
# declarations. Nothing may be silently relabeled as a MIR/native backend.
unsupported = [
    'extern fn foreign()->i64;fn main(){}',
    'fn main(){println("text");}',
    'fn main(){let f=fn(x:i64)->i64{return x;};println(f(1));}',
    'fn main(){let (a,b)=(1,2);println(a+b);}',
    'fn main(){let root=json_parse_or_die("{}");json_print(root);}',
    'fn main(){let code=quote{42};println(run(code));}',
    'fn main(){let b=buffer_new();buffer_append(b,"x");}',
]
for n, text in enumerate(unsupported):
    source = tests.source(f'unsupported_{n}', text)
    for mode in ['--emit-ir', '--emit-ir-c', '--ir-cfg', '--run-ir']:
        out = tests.work/'preserved'
        out.write_text('keep me')
        p = run([tests.compiler, mode, source, '-o', out])
        assert p.returncode == 1 and 'ON1X-MIR-UNSUPPORTED' in p.stderr, (text, p.stderr)
        assert out.read_text() == 'keep me'

source = tests.source('infinite', 'fn main(){while true{}}')
p = run([tests.compiler, '--run-ir', source])
assert p.returncode == 75 and 'budget exhausted' in p.stderr
for option in ['--backend=native', '--backend=vm', '--target=imaginary']:
    p = run([tests.compiler, option, source])
    assert p.returncode != 0 and 'unknown option' in p.stderr

# C++ API exercises wire round-trips, malformed IR and an independently authored
# phi loop; execute the latter's emitted C to catch sequential-copy regressions.
phi = tests.work/'phi.c'
p = run([tests.compiler.parent/'on1x_mir_tests', phi])
assert p.returncode == 0, p.stderr
p = native(phi, tests.work/'phi')
assert p.returncode == 0 and p.stdout == '20\n10\n', (p.returncode, p.stdout, p.stderr)

malformed = tests.work/'invalid.mir'
malformed.write_text('(on1x.mir 999 1 ())')
out = tests.work/'preserved'
out.write_text('keep me')
p = run([tests.compiler, '--ir-input', '--emit-ir-c', malformed, '-o', out])
assert p.returncode == 1 and 'version' in p.stderr and out.read_text() == 'keep me'
p = run([tests.compiler, '--ir-input', '--emit-c', malformed])
assert p.returncode == 2 and 'requires a MIR mode' in p.stderr

# Independently invoke the public build helper on the scalar example.
import sys
exe = tests.work/'helper-mir'
p = run([sys.executable, tests.root/'tools/on1x-build.py', tests.root/'examples/mir.on1x',
         '--via-mir', '--on1xc', tests.compiler, '--cc', tests.cc, '-o', exe,
         *['--cflag='+flag for flag in tests.flags]])
assert p.returncode == 0, p.stderr
p = run([exe])
assert p.returncode == 0 and p.stdout == '1\n1\n2\n6\n24\n120\nfalse\n'
print('MIR three-way execution, malformed input, limits and deterministic artifacts PASS')
