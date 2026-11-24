#!/usr/bin/env python3
"""Intrinsic structural match: actual source execution and independent oracles."""
import os
import random
from pathlib import Path
from support import NativeTests, run

t = NativeTests()


def execute(name, source, expected, options=(), mode='--emit-c', extra=(), env=None):
    path = t.source(name, source)
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


def differential(name, source, expected):
    path = execute(name, source, expected)
    for options, suffix in [((), '_mir'), (('--optimize-ir',), '_opt')]:
        execute(name + suffix, source, expected, options, '--emit-ir-c')
        p = run([t.compiler, '--run-ir', path, *options])
        assert p.returncode == 0 and p.stdout == expected, (name, p.stdout, p.stderr)
    ir = t.work / (name + '.mir')
    p = run([t.compiler, '--emit-ir', path, '-o', ir])
    assert p.returncode == 0, p.stderr
    p = run([t.compiler, '--emit-ir', '--ir-input', ir])
    assert p.returncode == 0 and p.stdout == ir.read_text(), p.stderr
    return path


def rejected(name, source, error, mode='--emit-c', options=()):
    path, out = t.source(name, source), t.work / 'preserved.c'
    out.write_text('preserved')
    p = run([t.compiler, mode, path, *options, '-o', out])
    assert p.returncode and error in p.stderr and not p.stdout and out.read_text() == 'preserved', (name, p.stderr)


example = (t.root / 'examples/structural_match.on1x').read_text()
path = execute('example', example, '42\n8\n4\n')
execute('optimized_c', example, '42\n8\n4\n', extra=['-O2'])
rejected('aggregate_mir', example, 'ON1X-MIR-UNSUPPORTED', '--emit-ir')

scalar = '''
fn probe(n:i64)->i64{println(n);return n;}
fn guard(n:i64)->bool{println(10+n);return n>0;}
fn classify(n:i64)->i64{return match probe(n) with {
  (0 | 1) as v if guard(v) => v+40,
  0 => 7,
  v => {let v=v+2; v}
};}
fn choose(flag:bool)->i64{match flag with {true=>{return 9;},false=>{return 8;}};}
fn main(){println(classify(0));println(classify(1));println(classify(2));
 println(choose(false));
 println(match let n=probe(4) with {3=>0, rest=>n+1});
 println(match (true,2) with {(true,n)|(false,n)=>n+40});
 match false with {true=>1,false=>println(90)};
}'''
scalar_path = differential('scalar', scalar, '0\n10\n7\n1\n11\n41\n2\n4\n8\n4\n5\n42\n90\n')
for mode in ['--check-types', '--check-memory']:
    p = run([t.compiler, mode, scalar_path])
    assert p.returncode == 0 or (p.returncode == 2 and 'unavailable' in p.stderr), p.stderr
    if p.returncode:
        print(mode + ': UNSUPPORTED (optional SDK not configured)')

differential('nested', '''
fn main(){var value=0;
 for(var i=0;i<4;i=i+1){
  match i with {0=>{continue;},1=>{value=value+2;},2=>{break;},rest=>assert(false)};
 }
 println(value);
 println(match true with {true=>{let n=3; match n with {3=>n+1,rest=>0}},false=>0});
 match true with {true=>{match false with {true=>{return;},false=>{return;}};},false=>{return;}};
}''', '2\n4\n')

execute('shapes', '''
fn main(){let v=variant<sum{A:sum{Yes:bool,No:record{}},B:(i64,bool)}>(A,variant<sum{Yes:bool,No:record{}}>(Yes,true));
 println(match v with {A(Yes(true))=>1,A(Yes(false))=>2,A(No())=>3,B(n,_)=>n});
 println(match array[2,3] with {[1,_]=>0,[a,b]=>a*b});
 let empty:array<i64,0>=array[];println(match empty with {[]=>5});
 println(match "hello\\n" with {"hello\\n"=>6,rest=>0});
 println(match -0x8000000000000000 with {-9223372036854775808=>7,rest=>0});
 println(match 255u8 with {0xffu8=>8,rest=>0});
 println(match -0.0f32 with {0.0f32=>9,rest=>0});
 println(match record{x:2,y:true} with {record{y:false,..}=>0,record{x:n,..}=>n+8});
}''', '1\n6\n5\n6\n7\n8\n9\n10\n')

differential('order', '''
fn mark(n:i64)->i64{println(n);return n;}
fn pair(a:i64,b:i64){println(a);println(b);}
fn main(){var n=1;
 println(n + match true with {true=>{n=4;2},false=>0});
 pair(n,match false with {true=>0,false=>{n=8;5}});
 assert(true||match mark(90) with {rest=>false});
 assert(!(false&&match mark(91) with {rest=>true}));
 println(match (mark(6),mark(7)) with {(a,b)=>a+b});
}''', '3\n4\n5\n6\n7\n13\n')

# Reuse the real closure allocator; both arms and abandoned nested operands drop.
extra = ['-include', t.root / 'tests/fixtures/closure_allocator.h',
         t.root / 'tests/fixtures/closure_allocator.c']
execute('owned_results', '''
extern fn live_environments()->i64;
fn make(n:i64)->fn()->i64{return fn()->i64{return n;};}
fn use(f:fn()->i64,n:i64){println(f()+n);}
fn early()->i64{return match true with {true=>{let f=make(5);return f();},false=>0};}
fn main(){
 {let f=match true with {true=>make(42),false=>make(0)};assert(live_environments()==1);println(f());}
 assert(live_environments()==0);
 {let pair=(make(2),make(3));let f=match pair with {(a,b) as both=>fn()->i64{return a()+b();}};println(f());}
 assert(live_environments()==0);
 outer:loop{use(make(10),match true with {true=>{let f=make(1);break outer;},false=>0});}
 assert(live_environments()==0);println(early());assert(live_environments()==0);
}''', '42\n5\n5\n', extra=extra)
p = run([t.work / 'owned_results'], env={**os.environ, 'ON1X_TEST_ENV_FAIL_AFTER': '0'})
assert p.returncode == 72 and 'allocation' in p.stderr, p.stderr
execute('foreign_arm', '''
extern fn on1x_external_double(x:i64)->i64;
fn main(){println(match true with {true=>on1x_external_double(21),false=>0});}
''', '42\n', extra=[t.root / 'examples/external.c'])

hygiene = '''
macro choose{rule($v:Expression)=>expression{match $v with {(x,y)|(y,x)=>x+y}};}
fn main(){let x=99;println(choose!((20,22)));println(x);
 let c=quote{match (lift(x),true) with {(n,true)=>n+1,(_,false)=>0}};println(run(c));
}'''
execute('hygiene', hygiene, '42\n99\n100\n')
execute('hygiene_ppx', hygiene, '42\n99\n100\n', ['--stdppx', 'identity'])
t.source('pattern_module', 'fn pick(n:i64)->i64{return match n with {1=>41,v=>v};}')
execute('imports', 'import pattern_module as M;fn main(){println(match 1 with {M=>M+1});println(M.pick(1));}', '2\n41\n')
execute('generic', 'fn pick(x)->_{return match x with {value=>value};}fn main(){println(pick(42));println(pick(true));}', '42\ntrue\n')
execute('identifiers', 'fn main(){let match=42;println(match);let with=1;let rest=2;println(with+rest);println(match+with);println(match-with);println(match==with);match;}', '42\n3\n43\n41\nfalse\n')

for name, options in [('python', ['--stdppx', 'identity']),
                      ('native', ['--ppx', 'identity', t.compiler.parent / 'stdppx/on1x-ppx-identity'])]:
    if name == 'native' and not Path(options[-1]).exists():
        continue
    execute('ppx_' + name, example, '42\n8\n4\n', options,
            env={**os.environ, 'ON1X_PPX_SCHEMA': str(t.root / 'grammar/PPX.schema.json')})

negative = [
    ('empty', 'match true with {}', 'empty match'),
    ('nonexhaustive', 'match true with {true=>1}', 'non-exhaustive'),
    ('infinite', 'match 1 with {1=>1,2=>2}', 'non-exhaustive'),
    ('guard_coverage', 'match true with {_ if true==false=>1}', 'non-exhaustive'),
    ('guard_type', 'match 1 with {_ if 1=>1,rest=>0}', 'expected bool'),
    ('unreachable', 'match true with {x=>1,false=>2}', 'unreachable'),
    ('duplicate', 'match 1 with {1=>1,0x1=>2,rest=>3}', 'redundant'),
    ('negative_zero', 'match 0.0 with {0.0=>1,-0.0=>2,rest=>3}', 'redundant'),
    ('rest_first', 'match 1 with {rest=>1,_=>2}', 'final'),
    ('rest_nested', 'match (1,2) with {(rest,_)=>1}', 'complete final-arm'),
    ('nonlinear', 'match (1,2) with {(x,x)=>x}', 'duplicate'),
    ('or_names', 'match (1,2) with {(x,_)|(_,y)=>x}', 'same names'),
    ('or_types', 'match (1,true) with {(x,_)|(_,x)=>1}', 'expected'),
    ('or_duplicate', 'match (1,2) with {(x,y)|(x,x)=>1}', 'duplicate'),
    ('literal_type', 'match 1 with {1u8=>1,rest=>2}', 'expected'),
    ('unsigned_negative', 'match 1u8 with {-1u8=>1,rest=>2}', 'unsigned literal'),
    ('nul_string', 'match "x" with {"\\0"=>1,rest=>2}', 'NUL'),
    ('tuple_arity', 'match (1,2) with {(x,y,z)=>1}', 'arity'),
    ('array_extent', 'match array[1] with {[x,y]=>1}', 'extent'),
    ('record_closed', 'match record{x:1,y:2} with {record{x:a}=>a}', 'every field'),
    ('unknown_field', 'match record{x:1} with {record{y:a,..}=>a}', 'unknown'),
    ('list_cons', 'match array[1,2] with {[x::tail]=>x,rest=>0}', 'owning list'),
    ('unknown_tag', 'match variant<sum{A:i64}>(A,1) with {B(n)=>n}', 'unknown constructor'),
    ('void_scrutinee', 'match println(1) with {_=>1}', 'cannot be void'),
    ('unique', 'match json_parse_or_die("{}") with {_=>1}', 'unique resources'),
]
for name, expression, error in negative:
    rejected(name, 'fn main(){' + expression + ';}', error)
rejected('result_types', 'fn main(){let x=match true with {true=>1,false=>false};}', 'expected')
rejected('void_arm', 'fn main(){let x=match true with {true=>{1;},false=>1};}', 'expected')
rejected('arm_escape', 'fn main(){match 1 with {n=>println(n)};println(n);}', 'undefined')
rejected('discriminant_escape', 'fn main(){match let n=1 with {_=>println(n)};println(n);}', 'undefined')
rejected('depth', 'fn main(){match true with {' + '(' * 300 + '_' + ')' * 300 + '=>0};}', 'limit')
rejected('expansion_budget', 'fn main(){match (' + ','.join(['true'] * 17) + ') with {(' + ','.join(['(true|false)'] * 17) + ')=>0};}', 'budget')

trap = 'fn guard(n:i64)->bool{return 1/n>0;}fn main(){println(91);match 0 with {n if guard(n)=>println(1),rest=>println(2)};}'
trap_path = t.source('guard_trap', trap)
for mode, suffix in [('--emit-c', 'hir'), ('--emit-ir-c', 'mir')]:
    out, exe = t.work / ('trap_' + suffix + '.c'), t.work / ('trap_' + suffix)
    p = run([t.compiler, mode, trap_path, '-o', out])
    assert p.returncode == 0, p.stderr
    p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', t.root / 'runtime', out,
             t.root / 'runtime/on1x_runtime.c', *t.flags, '-o', exe])
    assert p.returncode == 0, p.stderr
    p = run([exe])
    assert p.returncode == 70 and p.stdout == '91\n' and 'division' in p.stderr, p.stderr
p = run([t.compiler, '--run-ir', trap_path])
assert p.returncode == 70 and p.stdout == '91\n' and 'division' in p.stderr, p.stderr

if (t.root.parent / 'kernels/mulib-json.h').exists():
    source = '''
fn main(){let root=json_parse_or_die("{\\"n\\":42}");
 println(match true with {true=>{let view=json_get_or_die(root,"n");json_number(view)},false=>0});
 for(var i=0;i<3;i=i+1){match i with {0=>{let a=json_parse_or_die("{}");continue;},rest=>{let b=json_parse_or_die("{}");break;}};}
 match true with {true=>{let inner=json_parse_or_die("{}");return;},false=>{return;}};
}'''
    json_path = execute('json_cleanup', source, '42\n', extra=['-I', t.root.parent / 'kernels',
                        t.root / 'runtime/on1x_json.c', t.root / 'runtime/on1x_mulib_impl.c', '-lm'])
    for mode in ['--check-types', '--check-memory']:
        p = run([t.compiler, mode, json_path])
        assert p.returncode == 0 or (p.returncode == 2 and 'unavailable' in p.stderr), p.stderr

# Independent seeded oracle for ordered tuple patterns and effectful guards.
rng = random.Random(20261117)
for case in range(6):
    pairs = [(rng.randrange(-2, 3), bool(rng.randrange(2))) for _ in range(8)]
    calls = ''.join(f'println(pick(({n},{str(b).lower()})));' for n, b in pairs)
    source = 'fn pick(p:(i64,bool))->i64{return match p with {(0,_)=>10,(n,true) if n>0=>20+n,(n,_)=>30+n};}fn main(){' + calls + '}'
    expected = ''.join(str(10 if n == 0 else 20 + n if b and n > 0 else 30 + n) + '\n' for n, b in pairs)
    differential('oracle_' + str(case), source, expected)

print('Intrinsic structural match: C11/MIR oracles, guards, scopes, cleanup and closed profiles PASS')
