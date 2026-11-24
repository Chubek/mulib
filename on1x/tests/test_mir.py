#!/usr/bin/env python3
"""Five-way execution oracle: HIR C11, plain/optimized MIR C11 and evaluators."""
import hashlib
import random
import sys
from support import NativeTests, run

tests = NativeTests()
sys.path.insert(0, str(tests.root/'stdlib'))
import sexpr


def native(path, exe):
    p = run([tests.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-I', tests.root/'runtime', path, tests.root/'runtime/on1x_runtime.c',
             *tests.flags, '-o', exe])
    assert p.returncode == 0, p.stderr
    return run([exe])


def compare(name, text=None, source=None, expected=None, status=0, diagnostic=None, options=(),
            saved_native=False):
    source = source or tests.source(name, text)
    outputs = []
    for mode, optimization, label in [('--emit-c', [], '--emit-c'), ('--emit-ir-c', [], '--emit-ir-c'),
                                      ('--emit-ir-c', ['--optimize-ir'], '--optimized-c')]:
        path = tests.work/(name+label+'.c')
        p = run([tests.compiler, mode, source, *options, *optimization, '-o', path])
        assert p.returncode == 0, (name, p.stderr)
        again = run([tests.compiler, mode, source, *options, *optimization])
        assert again.returncode == 0 and path.read_text() == again.stdout
        p = native(path, tests.work/(name+label))
        outputs.append((p.returncode, p.stdout))
        assert p.returncode == status, (name, mode, p.returncode, p.stderr)
        if diagnostic:
            assert diagnostic in p.stderr, (name, mode, p.stderr)
    diagnostics = []
    for optimization in [[], ['--optimize-ir']]:
        p = run([tests.compiler, '--run-ir', source, *options, *optimization])
        outputs.append((p.returncode, p.stdout))
        diagnostics.append(p.stderr)
    assert all(output == outputs[0] for output in outputs), (name, outputs, p.stderr)
    assert diagnostics[0] == diagnostics[1], (name, diagnostics)
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
    if saved_native:
        for optimization, suffix in [([], '-saved'), (['--optimize-ir'], '-saved-opt')]:
            reread = run([tests.compiler, '--ir-input', '--run-ir', saved, *optimization])
            assert (reread.returncode, reread.stdout) == outputs[0], reread.stderr
            path = tests.work/(name+suffix+'.c')
            reread = run([tests.compiler, '--ir-input', '--emit-ir-c', saved, *optimization, '-o', path])
            assert reread.returncode == 0, reread.stderr
            reread = native(path, tests.work/(name+suffix))
            assert (reread.returncode, reread.stdout) == outputs[0], reread.stderr
            if diagnostic:
                assert diagnostic in reread.stderr, reread.stderr
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
compare('omitted_latch_continue', '''
fn mark(x:i64)->i64{println(x);return x;}
fn main(){
 outer:for(var i=mark(0);i<3;i=mark(i+1)){
  for(var j=0;j<2;j=loop{if true{continue outer;}break with 1;}){
   println(10+j);break;
  }
  break outer;
 }
 println(42);
}
''', expected='0\n10\n42\n', saved_native=True)
compare('entered_latch_continue', '''
fn step(x:i64)->i64{println(20+x);return x+1;}
fn main(){
 outer:for(var i=0;i<3;i=step(i)){
  for(var j=0;j<2;j=loop{if i<2{continue outer;}break with j+1;}){
   println(10+j);if i==2{break;}
  }
 }
 println(42);
}
''', expected='10\n20\n10\n21\n10\n22\n42\n', saved_native=True)
compare('latch_trap', '''
fn stop(n:i64)->i64{println(n);return 1/n;}
fn main(){for(var i=0;i<2;i=stop(i)){println(8);continue;}println(42);}
''', expected='8\n0\n', status=70, diagnostic='invalid division', saved_native=True)
compare('omitted_latch_tuple', '''
fn main(){for(var i=0;i<2;i=loop{let (a,b)=(4,5u16);break with a+u16_to_i64(b);}){
 println(i);break;
}println(42);}
''', expected='0\n42\n', saved_native=True)
assert (tests.work/'omitted_latch_tuple.mir').read_text().startswith('(on1x.mir 1 ')
compare('returns', '''
fn choose(n:i64)->i64{if n==1{return 10;}else{if n==2{return 20;}}return 30;}
fn find(n:i64)->i64{var i=0;while i<n{if i==3{return i;}i=i+1;}return -1;}
fn nothing(){return;}
fn main()->i64{nothing();println(choose(1));println(choose(2));println(choose(3));println(find(8));return 7;}
''', expected='10\n20\n30\n3\n', status=7)
compare('specialization', '''
fn identity(x)->_{return x;} fn main(){println(identity(42));println(identity(true));}
''', expected='42\ntrue\n')
compare('float_tuples', source=tests.root/'examples/float_tuples.on1x',
        expected='0x1.0000000000000p-1\n0x1.0000000000000p-3\n4\n')
compare('tuple_copy', '''
fn mark(x:i64)->i64 {println(x);return x;}
fn swap((a,(flag,b)):(i64,(bool,f64)))->(f64,(bool,i64)){return(b,(flag,a));}
fn recurse(x:(i64,f64),n:i64)->(i64,f64){if n==0{return x;}return recurse(x,n-1);}
fn main(){var pair=(mark(1),(true,2.5));let snapshot=pair;
pair=(mark(2),(false,-0.0));pair=pair;let(z,(flag,n))=swap(snapshot);
println(z);println(flag);println(n);let(a,b)=recurse((42,0.125),3);println(a);println(b);}
''', expected='1\n2\n0x1.4000000000000p+1\ntrue\n1\n42\n0x1.0000000000000p-3\n')
compare('float_edge', '''
fn number(n:i64)->f64{println(n);return f64_from_i64(n);}
fn main(){println(1.0e+0/number(0));println(-1E0/0.0);println(0.0/0.0);
let nan=0.0/0.0;println(nan==nan);println(nan!=nan);println(nan<0.0);
println(f64_to_bits(-0.0));println(f64_to_bits(5e-324));
println(f64_to_bits(1.7976931348623157e308));
println(f64_to_i64(-9223372036854775808.0));
println(number(1)+number(2));assert(true||number(99)==0.0);}
''', expected='0\ninf\n-inf\nnan\nfalse\ntrue\nfalse\n-9223372036854775808\n1\n9218868437227405311\n-9223372036854775808\n1\n2\n0x1.8000000000000p+1\n')
compare('float_cast_trap', 'fn main(){println(1);f64_to_i64(9223372036854775808.0);}',
        expected='1\n', status=70, diagnostic='f64 to i64 out of range')
compare('float_macro', '''
macro twice{rule($x:FloatLiteral)=>expression{$x+$x};}
fn main(){println(twice!(1.25));}
''', expected='0x1.4000000000000p+1\n')
compare('float_ppx_fold', 'fn main(){println((1.5+2.5)*2.0);}',
        expected='0x1.0000000000000p+3\n', options=['--stdppx','fold'])
# A source with an intentional identity rewrite carries advisory evidence,
# while its product-type payload is also executable as v2 without the sideband.
float_source = tests.source('directed_product', '''
fn add((a,b):(f64,f64))->f64{return a+b;}
fn main(){let flag=true;if true && flag{println(add((1.5,2.5)));}else{println(1/0);}}
''')
float_expected = '0x1.0000000000000p+2\n'
compare('directed_product', source=float_source, expected=float_expected, saved_native=True)
float_wire = (tests.work/'directed_product.mir').read_text()
float_tree = sexpr.loads(float_wire)
assert len(float_tree) == 5 and float_tree[:2] == ['on1x.mir', '4']
main_id = next(f[1] for f in float_tree[3] if f[2] == 'main')
assert ['directive', '', '', 'hint', 'canonicalized', main_id, 'false'] in float_tree[4]
assert ['directive', '', '', 'rewrite', 'fold-identity', main_id, 'false'] in float_tree[4]
passes = run([tests.compiler, '--dump-passes', float_source])
assert passes.returncode == 0, passes.stderr
report = sexpr.loads(passes.stdout)
assert report[2] == ['target', 'portable-directives-v4']
assert report[3] == ['source', float_wire]
assert sexpr.loads(report[4][1])[4] == float_tree[4]

legacy_v2 = tests.work/'float-compat-v2.mir'
legacy_wire = sexpr.dumps([float_tree[0], sexpr.Atom('2'), *float_tree[2:4]])
legacy_v2.write_text(legacy_wire)
saved = run([tests.compiler, '--ir-input', '--emit-ir', legacy_v2])
assert saved.returncode == 0 and saved.stdout == legacy_wire, saved.stderr
passes = run([tests.compiler, '--ir-input', '--dump-passes', legacy_v2])
assert passes.returncode == 0, passes.stderr
report = sexpr.loads(passes.stdout)
assert report[2] == ['target', 'portable-numeric-product-v2']
assert report[3] == ['source', legacy_wire]
for opts, suffix in [([], ''), (['--optimize-ir'], '-optimized')]:
    p = run([tests.compiler, '--ir-input', '--run-ir', legacy_v2, *opts])
    assert p.returncode == 0 and p.stdout == float_expected, p.stderr
    path = tests.work/('float-compat-v2'+suffix+'.c')
    p = run([tests.compiler, '--ir-input', '--emit-ir-c', legacy_v2, *opts, '-o', path])
    assert p.returncode == 0, p.stderr
    p = native(path, tests.work/('float-compat-v2'+suffix))
    assert p.returncode == 0 and p.stdout == float_expected, p.stderr
tests.source('mir_import', 'fn add(x:i64)->i64{return x+1;}')
compare('import', 'import "mir_import.on1x"; fn main(){println(add(41));}', expected='42\n')
tests.source('mir_numeric_import', 'fn duplicate(x)->_{return(x,x);}fn identity(x)->_{return x;}')
compare('numeric_import', '''
import "mir_numeric_import.on1x";fn main(){let(a,b)=duplicate(identity((42,1.5)));
let(x,y)=a;let(z,w)=b;println(x+z);println(y+w);}
''', expected='84\n0x1.8000000000000p+1\n')
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
    'fn main(){let (a,b)=(1,buffer_new());println(a);}',
    'fn main(){let root=json_parse_or_die("{}");json_print(root);}',
    'fn main(){let code=quote{42};println(run(code));}',
    'fn main(){let b=buffer_new();buffer_append(b,"x");}',
]
unsupported_latches = [
    # The absence of an incoming latch edge does not authorize unsupported HIR.
    'fn main(){for(var i=0;i<2;i=(fn()->i64{return 7;})()){println(i);break;}}',
    'fn main(){for(var i=0;i<2;println("dead step")){println(i);break;}}',
    'fn main(){for(var i=0;i<2;i=loop{defer println(7);break with 0;}){break;}}',
    'fn main(){var n=40;for(var i=0;i<2;i=loop{let a=borrow n;break with a;}){break;}}',
    '''record Cell{n:i64}
    fn main(){for(var i=0;i<2;i=loop{let x=box(record<Cell>{n:7});drop(x);break with 0;}){break;}}
    ''',
]
for n, text in enumerate(unsupported+unsupported_latches):
    source = tests.source(f'unsupported_{n}', text)
    if n >= len(unsupported):
        p = run([tests.compiler, '--emit-c', source])
        assert p.returncode == 0, (text, p.stderr)
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
p = native(tests.work/'phi.c.v2.c', tests.work/'phi-v2')
assert p.returncode == 0 and p.stdout == '20\n-0x0.0000000000000p+0\n10\n0x0.0000000000000p+0\n', p.stderr

malformed = tests.work/'invalid.mir'
malformed.write_text('(on1x.mir 999 1 ())')
out = tests.work/'preserved'
out.write_text('keep me')
p = run([tests.compiler, '--ir-input', '--emit-ir-c', malformed, '-o', out])
assert p.returncode == 1 and 'version' in p.stderr and out.read_text() == 'keep me'
entry_source=tests.source('aggregate_entry','fn value()->(i64,f64){return(1,2.0);}fn main(){}')
p=run([tests.compiler,'--emit-ir',entry_source]);assert p.returncode==0,p.stderr
bad_entry=tests.work/'aggregate-entry.mir'
bad_entry.write_text(p.stdout.replace('"main"','"unused"').replace('"value"','"main"'))
for mode in ['--run-ir','--emit-ir-c']:
    p=run([tests.compiler,'--ir-input',mode,bad_entry,'-o',out])
    assert p.returncode==1 and 'main must have no parameters and return i64 or void' in p.stderr and out.read_text()=='keep me',p.stderr
p = run([tests.compiler, '--ir-input', '--emit-c', malformed])
assert p.returncode == 2 and 'requires a MIR mode' in p.stderr

# Inject a syntactically well-shaped but unreachable block into the actual fixed
# artifact, rather than allowing lower's omission rule to leak into admission.
saved = tests.work/'omitted_latch_continue.mir'
wire = sexpr.loads(saved.read_text())
at = '(at "forged-latch.on1x" 0 1 1)'
main = next(f for f in wire[3] if f[2] == 'main')
main[6].append(sexpr.loads(f'(block 90001 90002 {at} () () (exit 90003 return 0 90002 () {at}))'))
malformed.write_text(sexpr.dumps(wire))
for mode in ['--emit-ir', '--run-ir', '--emit-ir-c']:
    p = run([tests.compiler, '--ir-input', mode, malformed, '-o', out])
    assert p.returncode == 1 and 'ON1X-MIR-VERIFY: unreachable block' in p.stderr, p.stderr
    assert not p.stdout and out.read_text() == 'keep me'

# Independently invoke the public build helper on the scalar example.
exe = tests.work/'helper-mir'
p = run([sys.executable, tests.root/'tools/on1x-build.py', tests.root/'examples/mir.on1x',
         '--via-mir', '--on1xc', tests.compiler, '--cc', tests.cc, '-o', exe,
         *['--cflag='+flag for flag in tests.flags]])
assert p.returncode == 0, p.stderr
p = run([exe])
assert p.returncode == 0 and p.stdout == '1\n1\n2\n6\n24\n120\nfalse\n'
print('MIR five-way execution, malformed input, limits and deterministic artifacts PASS')
