#!/usr/bin/env python3
"""Default pervasive Option/Result types, helpers, precedence and compatibility."""
from support import NativeTests, run


t = NativeTests()


def source(name, text):
    return t.source(name, text)


def execute(name, text, expected, options=()):
    path = source(name, text)
    out, exe = t.work / (name + '.c'), t.work / name
    p = run([t.compiler, '--emit-c', path, *options, '-o', out])
    assert p.returncode == 0, (name, p.stderr)
    p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-I', t.root / 'runtime', out, t.root / 'runtime/on1x_runtime.c',
             *t.flags, '-o', exe])
    assert p.returncode == 0, (name, p.stderr)
    p = run([exe])
    assert p.returncode == 0 and p.stdout == expected, (
        name, p.returncode, p.stdout, p.stderr, expected)
    return path


def reject(name, text, diagnostic, options=()):
    path = source(name, text)
    output = t.work / (name + '.preserved.c')
    output.write_text('preserved\n')
    p = run([t.compiler, '--emit-c', path, *options, '-o', output])
    assert p.returncode == 1 and diagnostic in p.stderr and not p.stdout, (
        name, p.returncode, p.stderr)
    assert output.read_text() == 'preserved\n'


# Every public helper executes through the ordinary generic nominal/C11 path.
# The fail_* callbacks establish that combinators only invoke selected branches.
execute('surface', r'''
fn increment(value:i64)->i64{return value+1;}
fn double(value:i64)->i64{return value*2;}
fn positive(value:i64)->bool{return value>0;}
fn even(value:i64)->bool{return value%2==0;}
fn negate(value:bool)->bool{return !value;}
fn subtract(left:i64,right:i64)->i64{return left-right;}
fn half(value:i64)->Option<i64>{
 if value%2==0{return option_some(value/2);}return option_none();
}
fn add_twenty_one(value:i64)->Result<i64,u8>{
 return result_ok(value+21);
}
fn recover(error:u8)->Result<i64,bool>{
 return result_ok(u8_to_i64(error)+35);
}
fn widen(error:u8)->i64{return u8_to_i64(error);}
fn nine()->i64{return 9;}
fn make_error()->bool{return true;}
fn mark_i64(value:i64){println(value);}
fn mark_u8(value:u8){println(value);}
fn fail_value(value:i64)->i64{tool_fail("unexpected callback");return value;}
fn fail_predicate(value:i64)->bool{tool_fail("unexpected callback");return value==0;}
fn fail_option(value:i64)->Option<i64>{tool_fail("unexpected callback");return option_some(value);}
fn fail_result(value:i64)->Result<i64,u8>{tool_fail("unexpected callback");return result_ok(value);}
fn fail_error(error:u8)->bool{tool_fail("unexpected callback");return error==0u8;}
fn fail_recover(error:u8)->Result<i64,bool>{tool_fail("unexpected callback");return result_error(error==0u8);}
fn fail_zero()->i64{tool_fail("unexpected callback");return 0;}
fn fail_make_error()->bool{tool_fail("unexpected callback");return false;}
fn fail_action(value:i64){tool_fail("unexpected callback");}
fn fail_error_action(value:u8){tool_fail("unexpected callback");}

fn checked(value:i64)->Result<i64,u8>{
 if value<0{return result_error(9u8);}return result_ok(value);
}
fn propagated(value:i64)->Result<i64,u8>{
 let item=checked(value)?;
 return result_ok(item*2);
}

fn main(){
 assert(identity(42)==42);assert(identity(true));ignore(0);
 assert(fst((40,2))==40);assert(snd((false,42))==42);
 assert(const(42,false)==42);assert(apply(double,21)==42);
 assert(compose(increment,double)(20)==41);
 assert(pipe(21,double)==42);assert(flip(subtract)(2,44)==42);

 let some=option_some(21);let none:Option<i64>=option_none();
 let no_bool:Option<bool>=option_none();
 assert(option_is_some(some)&&option_is_none(none)&&option_is_none(no_bool));
 assert(option_unwrap_or(option_map(some,double),0)==42);
 assert(option_is_none(option_map(none,fail_value)));
 assert(option_unwrap_or(option_and_then(option_some(84),half),0)==42);
 assert(option_is_none(option_and_then(none,fail_option)));
 assert(option_is_some(option_filter(some,positive)));
 assert(option_is_none(option_filter(some,even)));
 assert(option_is_none(option_filter(none,fail_predicate)));
 assert(option_unwrap_or(some,0)==21&&option_unwrap_or(none,42)==42);
 assert(option_unwrap_or_else(some,fail_zero)==21);
 assert(option_unwrap_or_else(none,nine)==9);
 let converted_ok:Result<i64,bool>=option_ok_or(some,false);
 let converted_error:Result<i64,bool>=option_ok_or(none,true);
 assert(result_unwrap_or(converted_ok,0)==21&&result_is_error(converted_error));
 assert(result_is_ok(option_ok_or_else(some,fail_make_error)));
 assert(result_is_error(option_ok_or_else(none,make_error)));
 option_iter(some,mark_i64);option_iter(none,fail_action);
 assert(option_unwrap_or(option_map(option_some(true),negate),true)==false);

 let ok:Result<i64,u8>=result_ok(21);
 let error:Result<i64,u8>=result_error(7u8);
 assert(result_is_ok(ok)&&result_is_error(error));
 assert(result_unwrap_or(result_map(ok,double),0)==42);
 assert(result_is_error(result_map(error,fail_value)));
 let unchanged:Result<i64,bool>=result_map_error(ok,fail_error);
 let changed:Result<i64,i64>=result_map_error(error,widen);
 assert(result_unwrap_or(unchanged,0)==21);
 assert(match changed with{Ok(_)=>false,Err(value)=>value==7});
 assert(result_unwrap_or(result_and_then(ok,add_twenty_one),0)==42);
 assert(result_is_error(result_and_then(error,fail_result)));
 let retained:Result<i64,bool>=result_or_else(ok,fail_recover);
 let recovered:Result<i64,bool>=result_or_else(error,recover);
 assert(result_unwrap_or(retained,0)==21);
 assert(result_unwrap_or(recovered,0)==42);
 assert(result_unwrap_or(ok,0)==21&&result_unwrap_or(error,42)==42);
 assert(result_unwrap_or_else(ok,widen)==21);
 assert(result_unwrap_or_else(error,widen)==7);
 assert(option_unwrap_or(result_ok_option(ok),0)==21);
 assert(option_is_none(result_ok_option(error)));
 assert(option_is_none(result_error_option(ok)));
 assert(option_unwrap_or(result_error_option(error),0u8)==7u8);
 result_iter(ok,mark_i64);result_iter(error,fail_action);
 result_iter_error(ok,fail_error_action);result_iter_error(error,mark_u8);
 let bool_ok:Result<bool,u8>=result_ok(true);assert(result_unwrap_or(bool_ok,false));

 assert(match propagated(21) with{Ok(value)=>value==42,Err(_)=>false});
 assert(match propagated(-1) with{Ok(_)=>false,Err(code)=>code==9u8});
 println(42);
}
''', '21\n21\n7\n42\n')

# Pervasive functions and type declarations are demand-loaded.
simple = source('demand_loaded', 'fn main(){println(42);}')
simple_c = t.work / 'demand_loaded.c'
p = run([t.compiler, '--emit-c', simple, '-o', simple_c])
assert p.returncode == 0, p.stderr
generated = simple_c.read_text()
assert 'basis_pervasive_' not in generated
assert 'on1x_fn_option_' not in generated and 'on1x_fn_result_' not in generated

# Source declarations shadow the automatic type and value namespaces, including
# a local Result that independently satisfies the existing ? protocol.
execute('local_shadow', '''
enum Option<T>{LocalNone,LocalSome(T)}
enum Result<T,E>{Ok(T),Err(E)}
fn option_some(value)->Option<_>{return variant<Option<_>>(LocalSome,value);}
fn option_none()->Option<_>{return variant<Option<_>>(LocalNone,record{});}
fn result_ok(value)->Result<_,_>{return variant<Result<_,_>>(Ok,value);}
fn result_error(error)->Result<_,_>{return variant<Result<_,_>>(Err,error);}
fn read()->Result<i64,u8>{return result_ok(41);}
fn use()->Result<i64,u8>{let value=read()?;return result_ok(value+1);}
fn main(){
 let value:Option<i64>=option_some(42);
 println(match value with{LocalNone()=>0,LocalSome(item)=>item});
 println(match use() with{Ok(item)=>item,Err(_)=>0});
}
''', '42\n42\n')

source('explicit_helpers', '''
fn option_map(value:i64,function:fn(i64)->i64)->i64{return function(value);}
''')
execute('explicit_import', '''
from explicit_helpers import option_map;
fn add_twenty_two(value:i64)->i64{return value+22;}
fn main(){println(option_map(20,add_twenty_two));}
''', '42\n')

custom = source('custom_prelude', '''
fn result_is_ok(value:i64)->bool{return value==42;}
''')
execute('custom_override', 'fn main(){println(result_is_ok(42));}',
        'true\n', ['--prelude', custom])

# --no-prelude removes both friendly values and nominal types, while direct
# imports of the same ordinary declarations remain available.
reject('disabled_value', 'fn main(){option_some(1);}', 'unknown function',
       ['--no-prelude'])
reject('disabled_type', 'fn take(value:Option<i64>){}fn main(){}', 'unknown type',
       ['--no-prelude'])
execute('explicit_core', '''
from std.core.Pervasive import Option, Result,
 basis_identity as identity,
 basis_pervasive_option_some as option_some,
 basis_pervasive_result_ok as result_ok;
fn main(){
 let value:Option<i64>=option_some(42);
 let outcome:Result<i64,u8>=result_ok(match value with{None()=>0,Some(n)=>n});
 println(match outcome with{Ok(n)=>identity(n),Err(_)=>0});
}
''', '42\n', ['--no-prelude'])

# Phantom constructors require contextual type arguments; wrong callbacks and
# managed nominal payloads continue to fail through ordinary inference/checking.
reject('infer_option_none', 'fn main(){let value=option_none();}',
       'cannot infer')
reject('infer_result_ok', 'fn main(){let value=result_ok(1);}',
       'cannot infer')
reject('infer_result_error', 'fn main(){let value=result_error(1);}',
       'cannot infer')
reject('wrong_callback', '''
fn wrong(value:bool)->bool{return value;}
fn main(){let value=option_map(option_some(1),wrong);}
''', 'expected i64')
reject('managed_payload', '''
fn main(){let value:Option<string>=option_some("text");}
''', 'trivially copied')
reject('option_propagation', '''
fn use()->Option<i64>{let value=option_some(1)?;return option_some(value);}
fn main(){}
''', 'exactly Ok(T)')

# Legacy tuple, scalar-pair and S-expression facades remain explicit, qualified
# APIs and do not change representation.
execute('legacy', '''
import std.Option as O;
import std.extensions.Result as R;
import std.base.option as SO;
import std.base.result as SR;
fn main(){
 assert(O.get_or(O.some(42),0)==42);
 assert(O.get_or(O.none(0),42)==42);
 assert(R.basis_result_unwrap_or(R.basis_result_ok(42),0)==42);
 let old_option=SO.base_some(sx_symbol("option"));
 assert(SO.base_is_some(old_option));
 assert(sx_text(SO.base_option_get(old_option))=="option");
 let old_result=SR.base_ok(sx_symbol("result"));
 assert(SR.base_result_ok(old_result));
 assert(sx_text(SR.base_result_payload(old_result))=="result");
 println(42);
}
''', '42\n')

# Existing explicit imports of implementation names and examples with their own
# Option/Result declarations remain admitted.
execute('legacy_core_names', '''
from std.core.Pervasive import basis_identity, basis_const;
fn main(){println(basis_identity(basis_const(42,false)));}
''', '42\n', ['--no-prelude'])
for example in ('nominal_adts.on1x', 'defer_errors.on1x'):
    p = run([t.compiler, '--check', t.root / 'examples' / example])
    assert p.returncode == 0, (example, p.stderr)

print('pervasive Option/Result helpers, precedence, inference and compatibility: PASS')
