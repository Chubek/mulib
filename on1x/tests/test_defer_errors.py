#!/usr/bin/env python3
"""Function-scoped defers and typed Result edges through the real C11 backend."""
import os
import random
from pathlib import Path
from support import NativeTests, run

t = NativeTests()
RESULT = 'enum Result<T,E>{Ok(T),Err(E)}\n'
env = {**os.environ, 'ON1X_PPX_SCHEMA': str(t.root / 'grammar/PPX.schema.json')}


def execute(name, text, expected, *, options=(), extra=(), environment=None):
    source = t.source(name, text)
    out, exe = t.work / (name + '.c'), t.work / name
    p = run([t.compiler, '--emit-c', source, *options, '-o', out], env=env)
    assert p.returncode == 0, (name, p.stderr)
    p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-I', t.root / 'runtime', out, t.root / 'runtime/on1x_runtime.c',
             *extra, *t.flags, '-o', exe])
    assert p.returncode == 0, (name, p.stderr)
    p = run([exe], env=environment)
    assert p.returncode == 0 and p.stdout == expected, (name, p.returncode, p.stdout, p.stderr, expected)
    return source, exe


def rejected(name, text, diagnostic, mode='--emit-c'):
    source, out = t.source(name, text), t.work / 'preserved.c'
    out.write_text('preserved')
    p = run([t.compiler, mode, source, '-o', out])
    assert p.returncode == 1 and diagnostic in p.stderr and not p.stdout and out.read_text() == 'preserved', (name, p.returncode, p.stderr)


example = (t.root / 'examples/defer_errors.on1x').read_text()
expected = '21\n99\n42\n-1\n99\n7\n2\n1\n0\n'
example_path, _ = execute('example', example, expected)
execute('example_o2', example, expected, extra=['-O2'])

# Callee first; operands snapshot once at registration, not at invocation.
execute('snapshots', '''
fn mark(n:i64)->i64{println(n);return n;}
fn pair(a:i64,b:i64){println(a*10+b);}
fn factory(n:i64)->fn(i64,i64)->void{println(n);return pair;}
fn recurse(n:i64)->i64{defer println(n);if n>0{return recurse(n-1)+1;}return 0;}
fn main(){var n=1;var f=pair;defer factory(mark(8))(mark(2),mark(3));
 defer f(n,4);n=9;f=fn(a:i64,b:i64){println(90);};
 {defer println(5);}println(6);println(recurse(2));
 let g=fn(){defer println(7);println(8);};g();println(9);
}''', '8\n8\n2\n3\n6\n0\n1\n2\n2\n8\n7\n9\n5\n14\n23\n')

execute('loop_scope', '''
fn choose()->i64{var n=10;defer println(n);n=20;return n;}
fn main(){outer:for i in 0..4{defer println(i);if i<2{continue outer;}break outer;}
 println(choose());println(50);if false{defer println(99);}
 defer fn(){defer println(70);println(60);}();
}''', '10\n20\n50\n60\n70\n2\n1\n0\n')

execute('raw_foreign', '''
extern fn defer_foreign((x,(enabled,text)):(i64,(bool,string)),extra:i64)->i64;
fn main(){var n=40;defer defer_foreign((n,(true,"ok")),2);n=0;println(n);}
''', '0\n42\n', extra=[t.root / 'tests/fixtures/defer_foreign.c'])
exit_exe = t.build('saved_main_exit', text='fn main()->i64{defer println(1);return 7;}')
p = run([exit_exe]); assert p.returncode == 7 and p.stdout == '1\n', p.stderr

execute('propagation', RESULT + '''
fn read(n:i64)->Result<i64,u8>{println(n);if n<0{return variant<Result<i64,u8>>(Err,3u8);}return variant<Result<i64,u8>>(Ok,n);}
fn mark(n:i64)->i64{println(n);return n;}
fn combine(a:i64,b:i64)->i64{return a+b;}
fn use(n:i64)->Result<bool,u8>{defer println(90);let value=combine(mark(1),read(n)?)+mark(2);
 return variant<Result<bool,u8>>(Ok,value==7);}
fn main(){println(match use(4) with{Ok(b)=>b,Err(_)=>false});
 println(match use(-1) with{Ok(_)=>0u8,Err(e)=>e});
 let f=fn()->Result<i64,u8>{defer println(80);let n=read(-2)?;return variant<Result<i64,u8>>(Ok,n);};
 println(match f() with{Ok(_)=>0u8,Err(e)=>e});
}''', '1\n4\n2\n90\ntrue\n1\n-1\n90\n3\n-2\n80\n3\n')

# Registration is not committed when an operand propagates; prior defers run.
execute('registration_error', RESULT + '''
fn bad()->Result<i64,u8>{return variant<Result<i64,u8>>(Err,9u8);}
fn mark(n:i64)->i64{println(n);return n;}
fn pair(a:i64,b:i64){println(a+b);}
fn use()->Result<i64,u8>{defer println(7);defer pair(mark(1),bad()?);return variant<Result<i64,u8>>(Ok,0);}
fn main(){println(match use() with{Ok(n)=>n,Err(e)=>u8_to_i64(e)});}
''', '1\n7\n9\n')

execute('control_edges', RESULT + '''
fn read(n:i64)->Result<i64,u8>{if n<0{return variant<Result<i64,u8>>(Err,4u8);}return variant<Result<i64,u8>>(Ok,n);}
fn use(n:i64)->Result<i64,u8>{defer println(9);
 let b=true||(read(-1)?>0);assert(b);assert(!(false&&(read(-2)?>0)));
 let v=loop{break with match n with{0=>read(-3)?,_=>read(n)?};};
 return variant<Result<i64,u8>>(Ok,v);}
fn main(){println(match use(0) with{Ok(n)=>n,Err(e)=>u8_to_i64(e)});
 println(match use(42) with{Ok(n)=>n,Err(e)=>u8_to_i64(e)});}
''', '9\n4\n9\n42\n')

# Numeric aggregates and chained postfix propagation retain exact types.
execute('aggregate_results', RESULT + '''
record Pair{x:i64,y:bool}
fn get()->Result<Pair,u8>{return variant<Result<Pair,u8>>(Ok,record<Pair>{x:42,y:true});}
fn nested()->Result<Result<array<i64,2>,u8>,u8>{return variant<Result<Result<array<i64,2>,u8>,u8>>(Ok,variant<Result<array<i64,2>,u8>>(Ok,array[8,9]));}
fn use()->Result<i64,u8>{let p=get()?;assert(p["y"]);return variant<Result<i64,u8>>(Ok,p["x"]+nested()??[1]);}
fn main(){println(match use() with{Ok(n)=>n,Err(_)=>0});}
''', '51\n')

execute('generic_protocol', '''enum Outcome<T,E>{Err(E),Ok(T)}
fn forward(r:Outcome<_,_>)->Outcome<_,_>{defer println(1);let n=r?;return variant<Outcome<_,_>>(Ok,n);}
fn ignored()->Outcome<i64,u8>{println(8);return variant<Outcome<i64,u8>>(Err,7u8);}
fn use()->Outcome<bool,u8>{defer ignored();return forward(variant<Outcome<bool,u8>>(Ok,true));}
fn main(){println(match use() with{Ok(b)=>b,Err(_)=>false});
 println(match forward(variant<Outcome<i64,u16>>(Ok,42)) with{Ok(n)=>n,Err(_)=>0});}
''', '1\n8\ntrue\n1\n42\n')

execute('nested_error_type', RESULT + '''
record Failure{code:u16,retry:bool}
fn read()->Result<record{},Failure>{return variant<Result<record{},Failure>>(Err,record<Failure>{code:7u16,retry:true});}
fn use()->Result<f32,Failure>{defer println(2);let unit=read()?;return variant<Result<f32,Failure>>(Ok,1.5f32);}
fn main(){match use() with{Ok(n)=>println(n),Err(e)=>{assert(e["retry"]);println(e["code"]);}}}
''', '2\n7\n')

# Hygiene, generic specialization, modules, and native/Python PPX source transport.
hygiene = RESULT + '''
macro later{rule($x:Expression)=>block{let n=$x;defer println(n);};}
macro attempt{rule($x:Expression)=>expression{$x?};}
fn pick(x)->_{defer println(1);return x;}
fn use()->Result<i64,u8>{let n=attempt!(variant<Result<i64,u8>>(Ok,42));return variant<Result<i64,u8>>(Ok,n);}
fn main(){let n=99;later!(2);later!(3);println(n);println(pick(true));println(pick(8));
 let c=quote{fn()->i64{defer println(4);return 5;}()};println(run(c));
 println(match use() with{Ok(n)=>n,Err(_)=>0});}
'''
hygiene_expected = '99\n1\ntrue\n1\n8\n4\n5\n42\n3\n2\n'
execute('hygiene', hygiene, hygiene_expected)
for name, options in [('python', ['--stdppx', 'identity']),
                      ('native', ['--ppx', 'identity', t.compiler.parent / 'stdppx/on1x-ppx-identity'])]:
    if name == 'native' and not Path(options[-1]).exists():
        print('Native identity PPX: UNSUPPORTED (native processors disabled)')
        continue
    execute('ppx_' + name, hygiene, hygiene_expected, options=options)
t.source('outcomes', RESULT + '''fn get()->Result<i64,u8>{defer println(6);return variant<Result<i64,u8>>(Ok,42);}''')
execute('imports', '''import outcomes as O;fn use()->O.Result<i64,u8>{defer println(7);let n=O.get()?;return variant<O.Result<i64,u8>>(Ok,n);}
fn main(){println(match use() with{Ok(n)=>n,Err(_)=>0});}''', '6\n7\n42\n')

# RC captures stay alive after their lexical block/iteration exits. Discarded
# managed results and abandoned earlier operands must also be released.
allocator = ['-include', t.root / 'tests/fixtures/closure_allocator.h',
             t.root / 'tests/fixtures/closure_allocator.c']
source, managed_exe = execute('managed', RESULT + '''
extern fn live_environments()->i64;
fn make(n:i64)->fn()->i64{return fn()->i64{return n;};}
fn consume(f:fn()->i64,b:buffer,s:sexpr){println(f());println(buffer_text(b));println(sx_text(s));}
fn give(n:i64)->fn()->i64{return make(n);}
fn bad()->Result<i64,u8>{return variant<Result<i64,u8>>(Err,3u8);}
fn abandon(f:fn()->i64,n:i64)->i64{return f()+n;}
fn use()->Result<i64,u8>{defer println(10);let n=abandon(make(9),bad()?);return variant<Result<i64,u8>>(Ok,n);}
fn scope(){let b=buffer_new();buffer_append(b,"a");
 {let s=sx_symbol("saved");defer consume(make(42),b,s);defer give(5);}
 buffer_append(b,"b");println(1);}
fn main(){scope();assert(live_environments()==0);assert(data_live_nodes()==0);
 println(match use() with{Ok(n)=>n,Err(e)=>u8_to_i64(e)});
 assert(live_environments()==0);assert(data_live_nodes()==0);
 for i in 0..100{let f=fn(){defer give(i);};f();}assert(live_environments()==0);}
''', '1\n42\nab\nsaved\n10\n3\n', extra=allocator)

# Faults remain terminal; suppress only LSan's expected fatal-path leaks, not
# ASan/UBSan checking. Every allocation before success is fault-injected.
fatal_env = {**os.environ, 'ASAN_OPTIONS': os.environ.get('ASAN_OPTIONS', '') + ':detect_leaks=0'}
for limit in range(6):
    p = run([managed_exe], env={**fatal_env, 'ON1X_TEST_ENV_FAIL_AFTER': str(limit)})
    assert p.returncode == 72 and 'allocation' in p.stderr and 'Sanitizer' not in p.stderr, (limit, p.returncode, p.stderr)

_, fault_exe = execute('registration_faults', '''extern fn live_environments()->i64;
fn f(){defer println(1);defer fn(){println(2);}();}
fn main(){f();assert(live_environments()==0);}''', '2\n1\n', extra=allocator)
for limit in range(4):
    # Earlier committed registrations intentionally survive a terminal failure;
    # an uncommitted record must nevertheless be freed after capture OOM.
    p = run([fault_exe], env={**fatal_env, 'ON1X_TEST_ENV_FAIL_AFTER': str(limit),
                            'ON1X_TEST_ENV_EXPECT_LIVE': '0' if limit < 2 else '2'})
    assert p.returncode == 72 and 'registration_faults.on1x:2:' in p.stderr and 'On1x defer error:' in p.stderr and 'allocation failure' in p.stderr and 'Sanitizer' not in p.stderr, (limit, p.stderr)
p = run([fault_exe], env={**os.environ, 'ON1X_TEST_ENV_FAIL_AFTER': '4'})
assert p.returncode == 0 and p.stdout == '2\n1\n', p.stderr

execute('saved_return', '''
fn get()->buffer{let b=buffer_new();buffer_append(b,"x");defer buffer_append(b,"z");return b;}
fn make(n:i64)->fn()->i64{defer println(1);return fn()->i64{return n;};}
fn main(){{let b=get();println(buffer_text(b));let f=make(42);println(f());}assert(data_live_nodes()==0);}
''', 'xz\n1\n42\n')

if (t.root.parent / 'include/mulib-json.h').exists():
    execute('json_cleanup', RESULT + '''
extern fn defer_json_blocks()->i64;
fn live(){assert(defer_json_blocks()>0);println(9);}
fn read(n:i64)->Result<i64,u8>{if n<0{return variant<Result<i64,u8>>(Err,7u8);}return variant<Result<i64,u8>>(Ok,n);}
fn use(n:i64)->Result<i64,u8>{let root=json_parse_or_die("{\\"n\\":42}");let view=json_get_or_die(root,"n");
 defer live();let value=json_number(view)+read(n)?;return variant<Result<i64,u8>>(Ok,value);}
fn main(){println(match use(0) with{Ok(n)=>n,Err(e)=>u8_to_i64(e)});assert(defer_json_blocks()==0);
 println(match use(-1) with{Ok(n)=>n,Err(e)=>u8_to_i64(e)});assert(defer_json_blocks()==0);
 for i in 0..3{let root=json_parse_or_die("42");defer println(json_number(root)+i);}assert(defer_json_blocks()==0);}
''', '9\n42\n9\n7\n44\n43\n42\n', extra=[
        '-I', t.root.parent / 'include', t.root / 'runtime/on1x_json.c',
        t.root / 'tests/fixtures/defer_json_allocator.c', '-lm'])

# Internal C ABI boundary, not separate On1x compilation or aggregate FFI.
cross_text = RESULT + '''
fn produce(n:i64)->Result<i64,u8>{defer println(100+n);if n<0{return variant<Result<i64,u8>>(Err,7u8);}return variant<Result<i64,u8>>(Ok,n+2);}
fn consume(n:i64)->i64{return match produce(n) with{Ok(v)=>v,Err(e)=>0-u8_to_i64(e)};}
fn main(){println(consume(40));}'''
cross_path, _ = execute('cross_tu', cross_text, '140\n42\n')
generated = (t.work / 'cross_tu.c').read_text()
interface = generated[:generated.index('struct on1x_lambda_env_')]
interface = '\n'.join(line for line in interface.splitlines() if not line.startswith('static '))
(t.work / 'defer_interface.h').write_text(interface)
strict = ['-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', t.root / 'runtime', *t.flags]
objects = []
for file in [t.work / 'cross_tu.c', t.root / 'runtime/on1x_runtime.c']:
    obj = t.work / (file.stem + '.o'); objects.append(obj)
    p = run([t.cc, '-std=c11', *strict, '-Dmain=on1x_test_hidden_main', '-c', file, '-o', obj])
    assert p.returncode == 0, p.stderr
for language, extension, standard in [('c', 'c', 'c11'), ('c++', 'cpp', 'c++20')]:
    caller = t.work / ('defer_caller.' + extension)
    header = '#include "defer_interface.h"\n'
    if language == 'c++': header = 'extern "C" {\n' + header + '}\n'
    caller.write_text(header + 'int main(void){return on1x_fn_consume(40)==42 && on1x_fn_consume(-1)==-7 ? 0 : 1;}\n')
    obj, exe = t.work / ('caller_' + extension + '.o'), t.work / ('cross_' + extension)
    p = run([t.cc, '-x', language, '-std=' + standard, *strict, '-c', caller, '-o', obj]); assert p.returncode == 0, p.stderr
    p = run([t.cc, *t.flags, *objects, obj, '-o', exe]); assert p.returncode == 0, p.stderr
    p = run([exe]); assert p.returncode == 0 and p.stdout == '140\n99\n', p.stderr

# Independently authored runtime caller: separate stacks, destruction, reuse,
# nested execution and exact source diagnostics on malformed allocation sizes.
runtime_exe = t.work / 'defer_runtime'
p = run([t.cc, '-std=c11', *strict, t.root / 'tests/test_defer_runtime.c',
         t.root / 'runtime/on1x_runtime.c', '-o', runtime_exe]); assert p.returncode == 0, p.stderr
p = run([runtime_exe]); assert p.returncode == 0, p.stderr
for invalid in ('overflow', 'bad_size'):
    p = run([runtime_exe, invalid]); assert p.returncode == 72 and 'runtime-test:42:7: On1x defer error:' in p.stderr, p.stderr

negative = [
    ('not_call', 'fn main(){defer 42;}', 'defer requires a call'),
    ('block', 'fn main(){defer {println(1);}}', 'defer block syntax is unsupported'),
    ('bad_arg', 'fn main(){defer println(fn(){});}', 'println accepts'),
    ('mutable_capture', 'fn main(){var n=1;defer fn(){println(n);}();}', 'capturing mutable'),
    ('resource_argument', 'fn main(){let r=json_parse_or_die("{}");defer json_print(r);}', 'defer cannot retain JSON'),
    ('resource_result', 'fn main(){defer json_parse_or_die("{}");}', 'defer cannot discard a JSON'),
    ('not_result', 'fn main(){let n=42?;}', 'nominal Result'),
    ('structural', 'fn f()->i64{return variant<sum{Ok:i64,Err:u8}>(Ok,1)?;}fn main(){}', 'nominal Result'),
    ('outside_result', RESULT+'fn main(){let n=variant<Result<i64,u8>>(Ok,1)?;}', 'same nominal Result'),
    ('wrong_error', RESULT+'fn f()->Result<i64,u16>{let n=variant<Result<i64,u8>>(Ok,1)?;return variant<Result<i64,u16>>(Ok,n);}fn main(){}', 'expected'),
    ('wrong_protocol', 'enum R<T,E>{Ok(E),Err(T)}fn f()->R<i64,u8>{let n=variant<R<i64,u8>>(Err,1)?;return variant<R<i64,u8>>(Err,n);}fn main(){}', 'exactly Ok(T)'),
    ('extra_variant', 'enum R<T,E>{Ok(T),Err(E),Other}fn f()->R<i64,u8>{let n=variant<R<i64,u8>>(Ok,1)?;return variant<R<i64,u8>>(Ok,n);}fn main(){}', 'exactly Ok(T)'),
    ('lambda_boundary', RESULT+'fn f()->Result<i64,u8>{let g=fn(){let n=variant<Result<i64,u8>>(Ok,1)?;};g();return variant<Result<i64,u8>>(Ok,1);}fn main(){}', 'same nominal Result'),
    ('option', 'enum Option<T>{None,Some(T)}fn main(){let n=variant<Option<i64>>(Some,1)?;}', 'exactly Ok(T)'),
    ('unused_bad', RESULT+'fn bad()->Result<i64,u8>{let n=1?;return variant<Result<i64,u8>>(Ok,n);}fn main(){}', 'nominal Result'),
    ('unreachable', 'fn main(){return;defer println(1);}', 'unreachable'),
]
for name, text, diagnostic in negative:
    rejected(name, text, diagnostic)
t.source('other_outcomes', RESULT)
rejected('module_identity', '''import outcomes as A;import other_outcomes as B;
fn f()->B.Result<i64,u8>{let n=A.get()?;return variant<B.Result<i64,u8>>(Ok,n);}fn main(){}''', 'same nominal Result')
rejected('postfix_depth', RESULT+'fn main(){let x=1'+'?'*300+';}', 'limit')

# Preserve ordinary identifiers/calls spelled defer, including whitespace.
execute('keyword_compat', '''fn defer(n:i64)->i64{return n+1;}fn main(){println(defer (41));let defer=9;defer;println(defer);}
''', '42\n9\n')

for mode in ('--emit-ir', '--emit-ir-c', '--run-ir'):
    rejected('mir_closed_' + mode[2:], 'fn main(){defer println(1);}', 'ON1X-MIR-UNSUPPORTED', mode)
    rejected('result_mir_' + mode[2:], example, 'ON1X-MIR-UNSUPPORTED', mode)
for mode in ('--check-types', '--check-memory'):
    p = run([t.compiler, mode, t.work / 'mir_closed_emit-ir.on1x'])
    assert p.returncode == 1 or (p.returncode == 2 and 'unavailable' in p.stderr), p.stderr
    if p.returncode == 2:
        print(mode + ': UNSUPPORTED (optional SDK not configured)')
    else:
        assert 'deferred' in p.stderr, p.stderr

# Dynamic record and byte budgets, including exact count boundary, report source.
limit_text = 'fn main(){for i in 0..4097{defer println(i);}println(9);}'
limit_exe = t.build('count_limit', text=limit_text)
p = run([limit_exe], env=fatal_env)
assert p.returncode == 72 and not p.stdout and 'defer error' in p.stderr and 'registration limit' in p.stderr and 'count_limit.on1x:1:' in p.stderr, p.stderr
execute('count_boundary', 'fn noop(){}fn main(){for i in 0..4096{defer noop();}}', '')
byte_exe = t.build('byte_limit', text='fn use(a:array<i64,256>){}fn main(){for i in 0..4096{defer use(array['+','.join('0' for _ in range(256))+']);}}')
p = run([byte_exe], env=fatal_env)
assert p.returncode == 72 and 'defer error' in p.stderr and 'capture byte limit' in p.stderr, p.stderr

# Explicit traps do not drain defers; a trap in a defer stops the remainder.
for name, text, expected in [
    ('body_trap', 'fn main(){defer println(9);println(1);var z=0;println(1/z);}', '1\n'),
    ('return_trap', 'fn f()->i64{defer println(9);var z=0;return 1/z;}fn main(){println(f());}', ''),
    ('defer_trap', 'fn main(){defer println(9);defer assert(false);println(1);}', '1\n'),
]:
    exe = t.build(name, text=text)
    p = run([exe], env=fatal_env)
    assert p.returncode == 70 and p.stdout == expected and 'Sanitizer' not in p.stderr, (name, p.returncode, p.stdout, p.stderr)

# Independent seeded outcome/LIFO oracle, at both C optimization levels.
rng = random.Random(2026111703)
for case in range(6):
    values = [rng.randrange(-3, 9) for _ in range(8)]
    stop = next((i for i, n in enumerate(values) if n < 0), len(values))
    trace = [100+i for i in range(stop+1 if stop<len(values) else stop)][::-1]
    outcome = 7 if stop<len(values) else sum(values)
    body = ''.join(f'defer println({100+i});total=total+read({n})?;' for i,n in enumerate(values))
    text = RESULT+'''fn read(n:i64)->Result<i64,u8>{if n<0{return variant<Result<i64,u8>>(Err,7u8);}return variant<Result<i64,u8>>(Ok,n);}
fn use()->Result<i64,u8>{var total=0;'''+body+'''return variant<Result<i64,u8>>(Ok,total);}
fn main(){println(match use() with{Ok(n)=>n,Err(e)=>u8_to_i64(e)});}'''
    output = ''.join(str(n)+'\n' for n in [*trace,outcome])
    for level in (0,2):
        execute(f'oracle_{case}_{level}', text, output, extra=[f'-O{level}'])

print('defer/Result: C11 execution, LIFO/outcome oracles, capture cleanup, fault/limit paths and closed boundaries PASS')
