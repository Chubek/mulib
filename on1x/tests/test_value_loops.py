#!/usr/bin/env python3
"""Value loops: actual C11, independent MIR execution, cleanup and boundaries."""
import os
import random
from pathlib import Path
from support import NativeTests, run

t = NativeTests()


def execute(name, text, expected, options=(), mode='--emit-c', extra=(), env=None):
    path = t.source(name, text)
    out, exe = t.work / (name + '.c'), t.work / name
    p = run([t.compiler, mode, path, *options, '-o', out], env=env)
    assert p.returncode == 0, (name, p.stderr)
    p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-I', t.root / 'runtime', out, t.root / 'runtime/on1x_runtime.c',
             *extra, *t.flags, '-o', exe])
    assert p.returncode == 0, (name, p.stderr)
    p = run([exe])
    assert p.returncode == 0 and p.stdout == expected, (name, p.stdout, p.stderr)
    return path


def differential(name, text, expected):
    path = execute(name, text, expected)
    for options, suffix in [((), '_mir'), (('--optimize-ir',), '_opt')]:
        execute(name + suffix, text, expected, options, '--emit-ir-c')
        p = run([t.compiler, '--run-ir', path, *options])
        assert p.returncode == 0 and p.stdout == expected, (name, p.stdout, p.stderr)
    ir = t.work / (name + '.mir')
    p = run([t.compiler, '--emit-ir', path, '-o', ir])
    assert p.returncode == 0, p.stderr
    p = run([t.compiler, '--emit-ir', '--ir-input', ir])
    assert p.returncode == 0 and p.stdout == ir.read_text(), p.stderr
    return path


def rejected(name, text, error, mode='--emit-c'):
    path, out = t.source(name, text), t.work / 'preserved.c'
    out.write_text('preserved')
    p = run([t.compiler, mode, path, '-o', out])
    assert p.returncode and error in p.stderr and not p.stdout and out.read_text() == 'preserved', (name, p.stderr)


def sdk(path):
    for mode in ['--check-types', '--check-memory']:
        p = run([t.compiler, mode, path])
        assert p.returncode == 0 or (p.returncode == 2 and 'unavailable' in p.stderr), p.stderr
        if p.returncode:
            print(mode + ': UNSUPPORTED (optional SDK not configured)')


example = (t.root / 'examples/value_loops.on1x').read_text()
path = differential('example', example, '49\n42\n8\n')
execute('optimized_c', example, '49\n42\n8\n', extra=['-O2'])
sdk(path)

differential('typed_joins', '''
fn choose(b:bool)->(i64,bool){return loop{if b{break with (42,true);}break with (9,false);};}
fn main(){let (n,b)=choose(true);assert(b);println(n);
 assert((loop{break with 1.5f32;})==1.5f32);
 assert((loop{break with -0.0;})==0.0);
 println(loop{if false{break with 0u64;}break with 18446744073709551615u64;});
}''', '42\n18446744073709551615\n')

outer = '''
fn step(n:i64)->i64{println(10+n);return n+1;}
fn choose(b:bool)->i64{return loop{if b{break with match true with {_=>{return 99;}};}break with 42;};}
fn main(){
 outer:for(var i=0;i<3;i=step(i)){
  let n=loop{if i<2{continue outer;}break with i+40;};println(n);
 }
 let answer=found:loop{let ignored=loop{if true{break found with 7;}break with 1;};break with ignored;};println(answer);
 println(choose(true));println(choose(false));
}'''
outer_path = differential('outer_exits', outer, '10\n11\n42\n12\n7\n99\n42\n')
sdk(outer_path)

differential('order', '''
fn mark(n:i64)->i64{println(n);return n;}
fn pair(a:i64,b:i64){println(a);println(b);}
fn main(){var n=1;println(n+loop{n=4;break with 2;});
 pair(n,loop{n=8;break with 5;});
 assert(true||loop{println(90);break with false;});
 assert(!(false&&loop{println(91);break with true;}));
 println(loop{break with mark(6)+mark(7);});
 (loop{if false{break;}break with println(9);});
}''', '3\n4\n5\n6\n7\n13\n9\n')

execute('value_shapes', '''
fn main(){let r=loop{break with record{x:42,y:true};};println(r["x"]);
 let a=loop{break with array[2,3,4];};println(a[1]);
 let s=loop{break with a[1:3];};println(s[1]);
 let v=loop{break with variant<sum{A:i64,B:bool}>(A,8);};
 println(match v with {A(n)=>n,B(_)=>0});
 println(loop{break with "value";});
}''', '42\n3\n4\n8\nvalue\n')
rejected('aggregate_mir', 'fn main(){let r=loop{break with record{x:1};};println(r["x"]);}', 'ON1X-MIR-UNSUPPORTED', '--emit-ir')

execute('identifier_compat', '''
fn break()->i64{return 9;}fn loop()->i64{return 2;}
fn main(){break;println(loop());println(loop{break with 42;});
 with:loop{break with;}
}''', '2\n42\n')

differential('order_and_void', '''
fn mark(n:i64)->i64{println(n);return n;}
fn pair(a:i64,b:i64){println(a);println(b);}
fn main(){var n=1;
 println(n+(loop{n=4;break with 2;}));
 pair(n,loop{n=8;break with mark(5);});
 assert(true||(loop{println(90);break with false;}));
 assert(!(false&&(loop{println(91);break with true;})));
 (loop{if false{break;}break with mark(6);});
 (loop{break with println(7);});
 with:loop{break with;}
 let break=9;println(loop{break with 42;});break;
}''', '3\n5\n4\n5\n6\n7\n42\n')

execute('aggregate_results', '''
fn main(){let r=loop{break with record{x:42,y:true};};println(r["x"]);assert(r["y"]);
 let a=loop{break with array[8,9];};println(a[0]);
 let s=loop{break with a[1:2];};println(s[0]);
 let v=loop{break with variant<sum{A:i64,B:bool}>(A,10);};println(match v with {A(n)=>n,B(_)=>0});
 println(loop{break with "text";});
}''', '42\n8\n9\n10\ntext\n')
rejected('aggregate_mir', 'fn main(){let a=loop{break with array[1];};println(a[0]);}', 'ON1X-MIR-UNSUPPORTED', '--emit-ir')

hygiene = '''
macro choose{rule($x:Expression)=>expression{local:loop{let n=$x;break local with n+1;}};}
macro wrap{rule($b:Block)=>block{local:loop{$b;break local;}};}
fn main(){let n=99;println(choose!(41));println(choose!(1));println(n);
 let result=local:loop{wrap!({if n==99{break local with 8;}});break with 0;};println(result);
 let c=quote{loop{break with lift(n)+1;}};println(run(c));
}'''
execute('hygiene_staging', hygiene, '42\n2\n99\n8\n100\n')
execute('hygiene_ppx', hygiene, '42\n2\n99\n8\n100\n', ['--stdppx', 'identity'])
execute('generic', '''
fn pick(x)->_{return loop{break with x;};}
fn main(){println(pick(42));println(pick(true));
 let local=fn(x)->_{return loop{break with x;};};println(local(8));println(local(false));}
''', '42\ntrue\n8\nfalse\n')
t.source('value_module', 'fn helper()->i64{return 42;}fn pick()->i64{return loop{let x=helper();break with x;};}')
execute('imports', 'import value_module as M;fn main(){println(loop{let M=9;break with M;});println(M.pick());}', '9\n42\n')
execute('foreign_boundary', '''
extern fn on1x_external_double(x:i64)->i64;
fn main(){println(loop{break with on1x_external_double(21);});}
''', '42\n', extra=[t.root / 'examples/external.c'])

# Values are retained before body cleanup; unfinished operands also drop.
extra = ['-include', t.root / 'tests/fixtures/closure_allocator.h',
         t.root / 'tests/fixtures/closure_allocator.c']
execute('closure_cleanup', '''
extern fn live_environments()->i64;
fn make(n:i64)->fn()->i64{return fn()->i64{return n;};}
fn use(f:fn()->i64,n:i64){println(f()+n);}
fn early(flag:bool)->i64{return loop{let f=make(3);if flag{return f();}break with f()+1;};}
fn main(){
 {let f=loop{let inner=make(42);break with inner;};assert(live_environments()==1);println(f());}
 assert(live_environments()==0);
 {let (a,b)=loop{break with (make(2),make(3));};println(a()+b());}
 assert(live_environments()==0);
 outer:for(var i=0;i<2;i=i+1){use(make(10),loop{let f=make(1);if i==0{continue outer;}break with f();});}
 assert(live_environments()==0);
 (loop{let f=make(6);if false{break;}break with f;});assert(live_environments()==0);
 println(early(true));println(early(false));assert(live_environments()==0);
}''', '42\n5\n11\n3\n4\n', extra=extra)
for limit in range(3):
    p = run([t.work / 'closure_cleanup'], env={**os.environ, 'ON1X_TEST_ENV_FAIL_AFTER': str(limit)})
    assert p.returncode == 72 and 'allocation' in p.stderr, (limit, p.returncode, p.stderr)

if (t.root.parent / 'kernels/mulib-json.h').exists():
    path = execute('json_cleanup', '''
fn main(){var n=0;println(loop{
 let root=json_parse_or_die("{\\"n\\":42}");let view=json_get_or_die(root,"n");
 n=n+1;if n<3{continue;}break with json_number(view);
});}''', '42\n', extra=['-I', t.root.parent / 'kernels', t.root / 'runtime/on1x_json.c',
                          t.root / 'runtime/on1x_mulib_impl.c', '-lm'])
    sdk(path)

for name, options in [('python', ['--stdppx', 'identity']),
                      ('native', ['--ppx', 'identity', t.compiler.parent / 'stdppx/on1x-ppx-identity'])]:
    if name == 'native' and not Path(options[-1]).exists():
        continue
    execute('ppx_' + name, example, '49\n42\n8\n', options,
            env={**os.environ, 'ON1X_PPX_SCHEMA': str(t.root / 'grammar/PPX.schema.json')})

negative = [
    ('heterogeneous', 'fn main(){let n=loop{if true{break with 1;}break with false;};}', 'expected'),
    ('no_coercion', 'fn main(){let n=loop{if true{break with 1u8;}break with 1u16;};}', 'expected'),
    ('bare_value', 'fn main(){let n=loop{if true{break;}break with 1;};}', 'void'),
    ('missing', 'fn main(){break with 1;}', 'no lexical target'),
    ('while_value', 'fn main(){while true{break with 1;}}', 'expression loop target'),
    ('for_value', 'fn main(){for(;;){break with 1;}}', 'expression loop target'),
    ('inner_statement', 'fn main(){let n=loop{loop{break with 1;}};}', 'expression loop target'),
    ('missing_label', 'fn main(){let n=loop{break missing with 1;};}', 'no lexical target'),
    ('lambda_escape', 'fn main(){let n=a:loop{let f=fn(){break a with 1;};break with 2;};}', 'no lexical target'),
    ('duplicate', 'fn main(){let n=a:loop{let x=a:loop{break with 1;};break with x;};}', 'duplicate active'),
    ('unreachable', 'fn main(){let n=loop{break with 1;println(2);};}', 'unreachable'),
    ('escaped_local', 'fn main(){let n=loop{let x=1;break with x;};println(x);}', 'undefined'),
    ('capture_mutable', 'fn main(){var x=1;let f=loop{break with fn()->i64{return x;};};}', 'capturing mutable'),
    ('unique_result', 'fn main(){let n=loop{break with json_parse_or_die("{}");};}', 'unique JSON'),
    ('unique_discard', 'fn main(){(loop{if false{break;}break with json_parse_or_die("{}");});}', 'unique JSON'),
    ('borrow_result', 'fn main(){let root=json_parse_or_die("{}");let n=loop{break with json_get_or_die(root,"n");};}', 'unique JSON'),
    ('msp_escape', 'fn main(){let n=1;let c=quote{loop{break with n;}};}', 'cross-stage'),
    ('macro_escape', 'macro b{rule()=>block{break with 1;};}fn main(){let n=loop{b!();break with 2;};}', 'no lexical target'),
]
for name, text, error in negative:
    rejected(name, text, error)
rejected('depth', 'fn main(){let x=' + 'loop{break with ' * 300 + '1' + ';}' * 300 + ';}', 'limit')

trap = 'fn main(){var zero=0;println(91);println(loop{break with 1/zero;});}'
trap_path = t.source('value_trap', trap)
exe = t.build('value_trap', source=trap_path)
p = run([exe])
assert p.returncode == 70 and p.stdout == '91\n' and 'division' in p.stderr, p.stderr
for options in [(), ('--optimize-ir',)]:
    p = run([t.compiler, '--run-ir', trap_path, *options])
    assert p.returncode == 70 and p.stdout == '91\n' and 'division' in p.stderr, p.stderr

# Seeded search oracle is independent of HIR lowering and MIR optimization.
rng = random.Random(2026111702)
for case in range(6):
    values = [rng.randrange(-4, 10) for _ in range(10)]
    bound = rng.randrange(1, 9)
    expected = next((n * n for n in values if n >= bound), -1)
    tests = ''.join(f'if {n}>={bound}{{break result with {n}*{n};}}' for n in values)
    source = 'fn main(){println(result:loop{' + tests + 'break with -1;});}'
    differential('oracle_' + str(case), source, str(expected) + '\n')

for case in range(4):
    limit, skip, divisor = rng.randrange(2, 16), rng.randrange(1, 10), rng.randrange(2, 7)
    expected = next((n * n for n in range(1, limit + 1) if n != skip and n % divisor == 0), -1)
    source = f'''
fn search(limit:i64,skip:i64,divisor:i64)->i64{{var n=0;return loop{{
 n=n+1;if n>limit{{break with -1;}}if n==skip{{continue;}}
 if n%divisor==0{{break with n*n;}}
}};}}
fn main(){{println(search({limit},{skip},{divisor}));}}'''
    differential('iteration_oracle_' + str(case), source, str(expected) + '\n')

print('Value loops: typed joins, C11/MIR oracles, lexical cleanup and closed boundaries PASS')
