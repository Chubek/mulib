#!/usr/bin/env python3
"""Recursive nominal ADTs through finite unique owning indirection."""
import os
from pathlib import Path

from support import NativeTests, run


t = NativeTests()
unique_fixture = (
    '-include', t.root / 'tests/fixtures/unique_allocator.h',
    t.root / 'tests/fixtures/unique_allocator.c',
)
managed_fixtures = unique_fixture + (
    '-include', t.root / 'tests/fixtures/closure_allocator.h',
    t.root / 'tests/fixtures/closure_allocator.c',
)


def reject(name, text, *diagnostics, mode='--check', extra=()):
    source = t.source('recursive_owning_reject_' + name, text)
    result = run([t.compiler, mode, source, *extra])
    assert result.returncode == 1 and not result.stdout, (
        name, result.returncode, result.stdout, result.stderr)
    for diagnostic in diagnostics:
        assert diagnostic in result.stderr, (name, diagnostic, result.stderr)
    return result


t.source('recursive_owning_library', r'''
enum List<T> { Nil, Cons(T, own<List<T>>) }
enum Choice<T> { Empty, Shared(sexpr), Owned(own<List<T>>) }
enum Outcome<T,E> { Ok(T), Err(E) }
record Bundle<T> { label:sexpr, values:own<List<T>> }

fn empty<T>()->own<List<T>> {
  return box(variant<List<T>>(Nil,record{}));
}
fn prepend<T>(item:T,tail:own<List<T>>)->own<List<T>> {
  return box(variant<List<T>>(Cons,(item,move(tail))));
}
fn sum(values:own<List<i64>>)->i64 {
  let node=take(values);
  return match node with {
    Nil()=>0,
    Cons(item,tail)=>item+sum(move(tail))
  };
}
fn bundle(label:sexpr,values:own<List<i64>>)->Bundle<i64> {
  return record<Bundle<i64>>{label:label,values:move(values)};
}
fn forward<T>(
    outcome:Outcome<own<List<T>>,own<List<T>>>
)->Outcome<own<List<T>>,own<List<T>>> {
  let values=move(outcome)?;
  return variant<Outcome<own<List<T>>,own<List<T>>>>(Ok,move(values));
}
''')

t.valid('recursive_owning_positive', '1\n2\n3\n', text=r'''
from recursive_owning_library import List, Choice, Outcome, Bundle, empty, prepend, sum, bundle, forward;
extern fn on1x_unique_live_allocations()->i64;
extern fn on1x_test_unique_allocator_live()->i64;
extern fn on1x_test_unique_allocator_allocations()->i64;

fn mark(value:i64)->i64 { println(value); return value; }
fn early_cleanup()->i64 {
  let values=prepend(8,empty());
  if true { return on1x_unique_live_allocations(); }
  return 0;
}
fn loop_cleanup()->i64 {
  let outer:own<List<i64>>=empty();
  loop {
    let inner:own<List<i64>>=prepend(1,empty());
    break;
  }
  return on1x_unique_live_allocations();
}
fn main(){
  let first:own<List<i64>>=empty();
  let second=prepend(mark(1),move(first));
  let third=prepend(mark(2),move(second));
  assert(on1x_unique_live_allocations()==3);
  println(sum(move(third)));
  assert(on1x_unique_live_allocations()==0);

  {
    let shared=variant<Choice<i64>>(Shared,sx_string("active arm"));
    assert(data_live_nodes()==1);
    drop(shared);
    assert(data_live_nodes()==0);
  }
  {
    let values:own<List<i64>>=empty();
    let owned=variant<Choice<i64>>(Owned,move(values));
    drop(owned);
    let empty_arm=variant<Choice<i64>>(Empty,record{});
    drop(empty_arm);
  }
  {
    let values:own<List<i64>>=empty();
    let wrapped=bundle(sx_string("shared"),move(values));
    assert(data_live_nodes()==1);
    drop(wrapped);
    assert(data_live_nodes()==0);
  }
  {
    var values:own<List<i64>>=empty();
    values=prepend(9,move(values));
    assert(on1x_unique_live_allocations()==2);
    values=empty();
    assert(on1x_unique_live_allocations()==1);
    drop(values);
  }
  {
    let result=forward(
      variant<Outcome<own<List<i64>>,own<List<i64>>>>(
        Ok,prepend(4,empty())));
    match result with {
      Ok(values)=>assert(sum(move(values))==4),
      Err(error)=>drop(error)
    };
  }
  {
    let result=forward(
      variant<Outcome<own<List<i64>>,own<List<i64>>>>(
        Err,prepend(5,empty())));
    match result with {
      Ok(values)=>drop(values),
      Err(error)=>assert(sum(move(error))==5)
    };
  }
  assert(early_cleanup()==2);
  assert(on1x_unique_live_allocations()==0);
  assert(loop_cleanup()==1);
  assert(on1x_unique_live_allocations()==0);
  assert(on1x_test_unique_allocator_live()==0);
  assert(on1x_test_unique_allocator_allocations()>0);
}
''', extra=managed_fixtures + ('-O2',))

t.valid('recursive_owning_results', '90\n90\n', text=r'''
enum Result<T,E> { Ok(T), Err(E) }
enum List { Nil, Cons(i64, own<List>) }
record Pair { first:own<List>, total:i64 }
extern fn on1x_unique_live_allocations()->i64;
extern fn on1x_test_unique_allocator_live()->i64;

fn empty()->own<List> {
  return box(variant<List>(Nil,record{}));
}
fn prepend(item:i64,tail:own<List>)->own<List> {
  return box(variant<List>(Cons,(item,move(tail))));
}
fn consume(values:own<List>)->i64 {
  var cursor=move(values);
  var total=0;
  return loop {
    let node=take(cursor);
    match node with {
      Nil()=>{break with total;},
      Cons(item,tail)=>{
        total=total+item;
        cursor=move(tail);
      }
    };
  };
}
fn read(ok:bool,value:i64)->Result<own<List>,own<List>> {
  if ok {
    return variant<Result<own<List>,own<List>>>(
      Ok,prepend(value,empty()));
  }
  return variant<Result<own<List>,own<List>>>(
    Err,prepend(value,empty()));
}
fn build(ok:bool)->Result<Pair,own<List>> {
  return variant<Result<Pair,own<List>>>(
    Ok,
    record<Pair>{
      first:prepend(7,empty()),
      total:consume(read(ok,11)?)
    });
}
fn forward(ok:bool)->Result<Pair,own<List>> {
  defer println(90);
  let pair=build(ok)?;
  return variant<Result<Pair,own<List>>>(Ok,move(pair));
}
fn nested(mode:i64)
    ->Result<Result<own<List>,own<List>>,own<List>> {
  if mode==0 {
    return variant<Result<Result<own<List>,own<List>>,own<List>>>(
      Err,prepend(30,empty()));
  }
  if mode==1 {
    return variant<Result<Result<own<List>,own<List>>,own<List>>>(
      Ok,variant<Result<own<List>,own<List>>>(
        Err,prepend(20,empty())));
  }
  return variant<Result<Result<own<List>,own<List>>,own<List>>>(
    Ok,variant<Result<own<List>,own<List>>>(
      Ok,prepend(10,empty())));
}
fn flatten(mode:i64)->Result<i64,own<List>> {
  let values=nested(mode)??;
  return variant<Result<i64,own<List>>>(Ok,consume(move(values)));
}
fn main(){
  let success=forward(true);
  match success with {
    Ok(pair)=>{
      assert(pair["total"]==11);
      drop(pair);
    },
    Err(values)=>drop(values)
  };
  assert(on1x_unique_live_allocations()==0);

  let failure=forward(false);
  match failure with {
    Ok(pair)=>drop(pair),
    Err(values)=>assert(consume(move(values))==11)
  };
  assert(on1x_unique_live_allocations()==0);

  let outer=flatten(0);
  match outer with {
    Ok(value)=>assert(value==0),
    Err(values)=>assert(consume(move(values))==30)
  };
  let inner=flatten(1);
  match inner with {
    Ok(value)=>assert(value==0),
    Err(values)=>assert(consume(move(values))==20)
  };
  let value=flatten(2);
  match value with {
    Ok(total)=>assert(total==10),
    Err(values)=>drop(values)
  };
  assert(on1x_unique_live_allocations()==0);
  assert(on1x_test_unique_allocator_live()==0);
}
''', extra=unique_fixture + ('-O2',))

# A move-only Result is itself consumed by `?`. Both the Ok owner and an owned
# Err payload transfer through canonical projections, while defers observe the
# documented pre-lexical-cleanup state.
owning_result_source = t.root / 'tests/fixtures/owning_result_lifecycle.on1x'
owning_result = t.valid(
    'owning_result_lifecycle',
    '100\n1\n200\n0\n20\n101\n1\n201\n0\n7\n102\n2\n202\n0\n8\n9\n',
    source=owning_result_source,
    extra=unique_fixture + ('-O2',))

# Terminal allocation failure is still distinct from recoverable Result::Err.
# Every allocation point in this Result-producing constructor is injected.
owning_result_fault_source = t.root / 'tests/fixtures/owning_result_fault.on1x'
owning_result_fault = t.build(
    'owning_result_fault',
    source=owning_result_fault_source,
    extra=unique_fixture)
for fail_after in ('0', '1'):
    failed = run([
        owning_result_fault
    ], env={**os.environ, 'ON1X_TEST_UNIQUE_FAIL_AFTER': fail_after})
    assert failed.returncode == 72 and \
           'owning-box allocation failure' in failed.stderr and \
           'Sanitizer' not in failed.stderr, (
               fail_after, failed.returncode, failed.stdout, failed.stderr)
successful = run([
    owning_result_fault
], env={**os.environ, 'ON1X_TEST_UNIQUE_FAIL_AFTER': '2'})
assert successful.returncode == 0 and not successful.stdout and \
       not successful.stderr, (
           successful.returncode, successful.stdout, successful.stderr)

# The generated ownership ABI is internal, but rebuilding the generated C and
# runtime together must remain usable from strict C11 and C++17 translation
# units. Callers consume the successful owner exactly once; the failing
# constructor has already cleaned its partial acquisition.
generated_result = t.work / 'owning_result_lifecycle.c'
generated_text = generated_result.read_text(encoding='utf-8')
interface_end = generated_text.index('static void on1x_unique_destroy_')
interface_text = '\n'.join(
    line for line in generated_text[:interface_end].splitlines()
    if not line.startswith('static ')) + '\n'
result_prototype = next(
    line for line in interface_text.splitlines()
    if ' on1x_fn_boundary_make(bool);' in line)
result_type = result_prototype.split()[0]
interface = t.work / 'owning_result_interface.h'
interface.write_text(interface_text, encoding='utf-8')
strict = [
    '-Wall', '-Wextra', '-Wpedantic', '-Werror',
    '-I', t.root / 'runtime', '-I', t.work, *t.flags,
]
objects = []
for name, file, extra in (
        ('owning_result_generated', generated_result,
         ('-Dmain=on1x_test_hidden_main',)),
        ('owning_result_runtime', t.root / 'runtime/on1x_runtime.c',
         ('-include', t.root / 'tests/fixtures/unique_allocator.h')),
        ('owning_result_allocator',
         t.root / 'tests/fixtures/unique_allocator.c', ())):
    obj = t.work / (name + '.o')
    compiled = run([
        t.cc, '-std=c11', *strict, *extra, '-c', file, '-o', obj])
    assert compiled.returncode == 0, (name, compiled.stderr)
    objects.append(obj)
for language, extension, standard in (
        ('c', 'c', 'c11'), ('c++', 'cpp', 'c++17')):
    caller = t.work / ('owning_result_caller.' + extension)
    include = '#include "owning_result_interface.h"\n'
    if language == 'c++':
        include = 'extern "C" {\n' + include + '}\n'
    caller.write_text(include + f'''
int main(void) {{
  {result_type} ok = on1x_fn_boundary_make(false);
  if (ok.tag != 1 || !ok.payload.element_1 ||
      on1x_unique_live_allocations() != 1) return 1;
  on1x_unique_drop(ok.payload.element_1);
  if (on1x_unique_live_allocations() != 0) return 2;
  {result_type} error = on1x_fn_boundary_make(true);
  if (error.tag != 0 || error.payload.element_0 != 4 ||
      on1x_unique_live_allocations() != 0) return 3;
  return 0;
}}
''', encoding='utf-8')
    caller_object = t.work / ('owning_result_caller_' + extension + '.o')
    compiled = run([
        t.cc, '-x', language, '-std=' + standard, *strict,
        '-c', caller, '-o', caller_object])
    assert compiled.returncode == 0, (language, compiled.stderr)
    executable = t.work / ('owning_result_boundary_' + extension)
    linked = run([
        t.cc, *t.flags, *objects, caller_object, '-o', executable])
    assert linked.returncode == 0, (language, linked.stderr)
    crossed = run([executable])
    assert crossed.returncode == 0 and not crossed.stdout and \
           not crossed.stderr, (
           language, crossed.returncode, crossed.stdout, crossed.stderr)

t.valid('recursive_owning_stress', '', text=r'''
enum List { Nil, Cons(i64, own<List>) }
extern fn on1x_unique_live_allocations()->i64;
extern fn on1x_test_unique_allocator_live()->i64;
fn empty()->own<List>{return box(variant<List>(Nil,record{}));}
fn prepend(item:i64,tail:own<List>)->own<List>{
  return box(variant<List>(Cons,(item,move(tail))));
}
fn consume(values:own<List>)->i64 {
  var cursor=move(values);
  var count=0;
  return loop {
    let node=take(cursor);
    match node with {
      Nil()=>{break with count;},
      Cons(_,tail)=>{count=count+1;cursor=move(tail);}
    };
  };
}
fn main(){
  {
    var values=empty();
    for i in 0..50000 { values=prepend(i,move(values)); }
    assert(on1x_unique_live_allocations()==50001);
    assert(on1x_test_unique_allocator_live()==50001);
    assert(consume(move(values))==50000);
    assert(on1x_unique_live_allocations()==0);
    assert(on1x_test_unique_allocator_live()==0);
  }
  {
    var values=empty();
    for i in 0..50000 { values=prepend(i,move(values)); }
    assert(on1x_unique_live_allocations()==50001);
  }
  assert(on1x_unique_live_allocations()==0);
  assert(on1x_test_unique_allocator_live()==0);
}
''', extra=unique_fixture + ('-O2',))

failure = t.build('recursive_owning_failure', text=r'''
enum Node { Leaf }
record Pair { label:sexpr, left:own<Node>, right:own<Node> }
fn leaf()->own<Node>{return box(variant<Node>(Leaf,record{}));}
fn through_call(value:own<Node>)->own<Node>{
  let temporary=leaf();
  drop(temporary);
  return move(value);
}
fn main(){
  let seed=leaf();
  let pair=record<Pair>{
    label:sx_string("live during failure"),
    left:move(seed),
    right:through_call(leaf())
  };
  drop(pair);
}
''', extra=managed_fixtures)
for fail_after in ('0', '1', '2'):
    failed = run(
        [failure],
        env={**os.environ, 'ON1X_TEST_UNIQUE_FAIL_AFTER': fail_after})
    assert failed.returncode == 72 and not failed.stdout, (
        fail_after, failed.returncode, failed.stdout, failed.stderr)
    assert 'owning-box allocation failure' in failed.stderr, failed.stderr
successful = run([failure])
assert successful.returncode == 0 and not successful.stdout and not successful.stderr, (
    successful.returncode, successful.stdout, successful.stderr)

result_failure = t.build('recursive_owning_result_failure', text=r'''
enum Result<T,E> { Ok(T), Err(E) }
enum Node { Leaf }
fn leaf()->own<Node> {
  return box(variant<Node>(Leaf,record{}));
}
fn allocate_in_defer(){
  let value=leaf();
  drop(value);
}
fn source()->Result<own<Node>,own<Node>> {
  return variant<Result<own<Node>,own<Node>>>(Err,leaf());
}
fn forward()->Result<own<Node>,own<Node>> {
  defer allocate_in_defer();
  let value=source()?;
  return variant<Result<own<Node>,own<Node>>>(Ok,move(value));
}
fn main(){
  let result=forward();
  match result with {
    Ok(value)=>drop(value),
    Err(error)=>drop(error)
  };
}
''', extra=unique_fixture)
for fail_after in ('0', '1'):
    failed = run(
        [result_failure],
        env={**os.environ, 'ON1X_TEST_UNIQUE_FAIL_AFTER': fail_after})
    assert failed.returncode == 72 and not failed.stdout and \
           'owning-box allocation failure' in failed.stderr, (
               fail_after, failed.returncode, failed.stdout, failed.stderr)
successful = run([result_failure])
assert successful.returncode == 0 and not successful.stdout and \
       not successful.stderr, (
           successful.returncode, successful.stdout, successful.stderr)

negative_cases = (
    ('implicit_copy',
     'enum N{Z}fn main(){let a=box(variant<N>(Z,record{}));let b=a;drop(a);drop(b);}',
     ('copy of unique local requires move(local)',)),
    ('use_after_move',
     'enum N{Z}fn sink(x:own<N>){drop(x);}fn main(){let a=box(variant<N>(Z,record{}));sink(move(a));drop(a);}',
     ('use after move',)),
    ('double_move',
     'enum N{Z}fn sink(x:own<N>){drop(x);}fn main(){let a=box(variant<N>(Z,record{}));sink(move(a));sink(move(a));}',
     ('use after move',)),
    ('move_temporary',
     'enum N{Z}fn main(){drop(move(box(variant<N>(Z,record{}))));}',
     ('move(local) requires a named local',)),
    ('move_copy',
     'fn main(){let n=1;println(move(n));}',
     ('move(local) requires a unique owning value',)),
    ('box_structural',
     'fn main(){drop(box(record{x:1}));}',
     ('box(value) requires a known nominal record or enum value',)),
    ('take_copy',
     'fn main(){println(take(1));}',
     ('owning operation requires own<Named>',)),
    ('drop_copy',
     'fn main(){drop(1);}',
     ('drop(owner) requires a unique owning value',)),
    ('branch_move',
     'enum N{Z}fn f(flag:bool){let a=box(variant<N>(Z,record{}));if flag{drop(a);}drop(a);}fn main(){}',
     ('use after move',)),
    ('loop_move',
     'enum N{Z}fn f(flag:bool){let a=box(variant<N>(Z,record{}));while flag{drop(a);}}fn main(){}',
     ('repeatable loop',)),
    ('propagate_use_after_move',
     'enum Result<T,E>{Ok(T),Err(E)}enum N{Z}'
     'fn f(r:Result<own<N>,u8>)->Result<i64,u8>{'
     'let value=move(r)?;drop(value);drop(r);'
     'return variant<Result<i64,u8>>(Ok,0);}fn main(){}',
     ('use after move',)),
    ('propagate_twice',
     'enum Result<T,E>{Ok(T),Err(E)}enum N{Z}'
     'fn f(r:Result<own<N>,u8>)->Result<i64,u8>{'
     'let first=move(r)?;let second=move(r)?;drop(first);drop(second);'
     'return variant<Result<i64,u8>>(Ok,0);}fn main(){}',
     ('use after move',)),
    ('capture',
     'enum N{Z}fn main(){let a=box(variant<N>(Z,record{}));let f=fn(){drop(a);};f();}',
     ('closures cannot capture unique owning values',)),
    ('guard',
     'enum L{Nil,Cons(own<L>)}fn main(){let n=take(box(variant<L>(Nil,record{})));match n with{Nil() if true=>0,Cons(x)=>{drop(x);0}};}',
     ('move-only match arms cannot have guards',)),
    ('defer',
     'enum N{Z}fn sink(x:own<N>){drop(x);}fn main(){let a=box(variant<N>(Z,record{}));defer sink(move(a));}',
     ('defer cannot retain unique owning values',)),
    ('foreign_parameter',
     'enum N{Z}extern fn consume(x:own<N>);fn main(){}',
     ('unique ownership cannot cross a raw foreign parameter ABI',)),
    ('foreign_result',
     'enum N{Z}extern fn make()->own<N>;fn main(){}',
     ('unique ownership cannot cross a raw foreign return ABI',)),
    ('result_foreign_parameter',
     'enum N{Z}enum Result<T,E>{Ok(T),Err(E)}extern fn consume(x:Result<own<N>,u8>);fn main(){}',
     ('unique ownership cannot cross a raw foreign parameter ABI',)),
    ('result_foreign_result',
     'enum N{Z}enum Result<T,E>{Ok(T),Err(E)}extern fn make()->Result<i64,own<N>>;fn main(){}',
     ('unique ownership cannot cross a raw foreign return ABI',)),
    ('result_implicit_copy',
     'enum N{Z}enum Result<T,E>{Ok(T),Err(E)}fn main(){let a=variant<Result<own<N>,u8>>(Ok,box(variant<N>(Z,record{})));let b=a;drop(a);drop(b);}',
     ('copy of unique local requires move(local)',)),
    ('generic_result_forward',
     'enum N{Z}enum Result<T,E>{Ok(T),Err(E)}fn forward<T,E>(r:Result<T,E>)->Result<T,E>{let value=r?;return variant<Result<T,E>>(Ok,value);}fn make()->Result<own<N>,u8>{return variant<Result<own<N>,u8>>(Ok,box(variant<N>(Z,record{})));}fn main(){drop(forward(make()));}',
     ('copy of unique local requires move(local)',)),
    ('pervasive_result_helper_owner',
     'enum N{Z}fn make()->Result<own<N>,u8>{let owner=box(variant<N>(Z,record{}));return result_ok(move(owner));}fn main(){drop(make());}',
     ('copy of unique local requires move(local)',)),
    ('direct_recursion',
     'enum L{Nil,Cons(i64,L)}fn main(){}',
     ('recursive nominal type has no owning indirection',)),
    ('array_owner',
     'enum N{Z}fn main(){let a=array[box(variant<N>(Z,record{}))];}',
     ('structural aggregates cannot contain unique owners',)),
    ('staging',
     'enum N{Z}fn main(){let code=quote{box(variant<N>(Z,record{}))};}',
     ('staged code cannot contain unique ownership',)),
)
for name, text, diagnostics in negative_cases:
    reject(name, text, *diagnostics)

double_propagate = run([
    t.compiler, '--check',
    t.root / 'tests/fixtures/owning_result_double_propagate.on1x'])
assert double_propagate.returncode == 1 and \
       'use after move of local: pending' in double_propagate.stderr and \
       not double_propagate.stdout, (
           double_propagate.returncode,
           double_propagate.stdout,
           double_propagate.stderr)

owner_source = t.source('recursive_owning_boundaries', r'''
enum Node { Leaf }
fn leaf()->own<Node>{return box(variant<Node>(Leaf,record{}));}
fn main(){let value:own<Node>=leaf();drop(value);}
''')
mir = run([t.compiler, '--emit-ir', owner_source])
assert mir.returncode == 1 and not mir.stdout and \
       'ON1X-MIR-UNSUPPORTED' in mir.stderr and \
       'typed move/drop and cleanup MIR events' in mir.stderr, mir.stderr

serialized = run([t.compiler, '--emit-ast', owner_source])
assert serialized.returncode == 1 and not serialized.stdout and \
       'ON1X-OWN-PPX' in serialized.stderr, serialized.stderr

ppx = run([
    t.compiler, '--check', owner_source,
    '--ppx', 'identity', t.root / 'stdppx/identity.py'])
assert ppx.returncode == 1 and not ppx.stdout and \
       'ON1X-OWN-PPX' in ppx.stderr, ppx.stderr

concurrent = reject(
    'concurrency',
    'enum N{Z}fn main(){let value=box(variant<N>(Z,record{}));taskgroup{}drop(value);}',
    'unique ownership cannot cross cooperative concurrency')

# The internal runtime ABI fails closed for malformed direct callers.
abi_source = t.work / 'recursive-owning-abi.c'
abi_source.write_text(r'''
#include <on1x_runtime.h>
#include <string.h>
typedef struct { int value; } payload;
static void destroy(void *raw){memset(raw,0,sizeof(payload));}
int main(int argc,char **argv){
  payload source={1},destination={0};
  on1x_unique *owner=0;
  if(argc!=2)return 3;
  if(argv[1][0]=='z'){
    (void)on1x_unique_try_box(&source,0,destroy);
  }else if(argv[1][0]=='t'){
    on1x_unique_take(&owner,&destination,sizeof(destination),"abi",1,1);
  }else if(argv[1][0]=='s'){
    owner=on1x_unique_try_box(&source,sizeof(source),destroy);
    if(!owner)return 6;
    on1x_unique_take(&owner,&destination,sizeof(destination),"abi",1,1);
    on1x_unique_take(&owner,&destination,sizeof(destination),"abi",1,2);
  }else if(argv[1][0]=='c'){
    on1x_cleanup record={0};
    on1x_cleanup_push(&record,0,destroy);
  }else return 4;
  return 5;
}
''', encoding='utf-8')
abi_executable = t.work / 'recursive-owning-abi'
compiled = run([
    t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
    '-I', t.root / 'runtime', abi_source, t.root / 'runtime/on1x_runtime.c',
    *t.flags, '-o', abi_executable])
assert compiled.returncode == 0, compiled.stderr
for mode, diagnostic in (
        ('zero', 'invalid owning-box allocation'),
        ('take', 'invalid take from owning box'),
        ('stale', 'invalid take from owning box'),
        ('cleanup', 'invalid cleanup registration')):
    malformed = run([abi_executable, mode])
    assert malformed.returncode == 72 and not malformed.stdout and \
           diagnostic in malformed.stderr, (mode, malformed.stderr)

# Generated code refuses a runtime header from a different unique ABI version.
generated = t.work / 'recursive-owning-boundaries.c'
emitted = run([t.compiler, '--emit-c', owner_source, '-o', generated])
assert emitted.returncode == 0, emitted.stderr
bad_include = t.work / 'bad-unique-abi'
bad_include.mkdir(exist_ok=True)
runtime_header = (t.root / 'runtime/on1x_runtime.h').read_text(encoding='utf-8')
runtime_header = runtime_header.replace(
    '#define ON1X_UNIQUE_ABI_VERSION 1',
    '#define ON1X_UNIQUE_ABI_VERSION 2')
(bad_include / 'on1x_runtime.h').write_text(runtime_header, encoding='utf-8')
abi_mismatch = run([
    t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
    '-I', bad_include, '-c', generated, '-o', t.work / 'bad-unique-abi.o'])
assert abi_mismatch.returncode != 0 and \
       'incompatible On1x unique ownership runtime ABI' in abi_mismatch.stderr, \
       abi_mismatch.stderr

print('recursive owning ADTs: moves, owning Results, cleanup, ABI, failures and boundaries PASS')
