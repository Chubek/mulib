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
''')

t.valid('recursive_owning_positive', '1\n2\n3\n', text=r'''
from recursive_owning_library import List, Choice, Bundle, empty, prepend, sum, bundle;
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
  assert(early_cleanup()==2);
  assert(on1x_unique_live_allocations()==0);
  assert(loop_cleanup()==1);
  assert(on1x_unique_live_allocations()==0);
  assert(on1x_test_unique_allocator_live()==0);
  assert(on1x_test_unique_allocator_allocations()>0);
}
''', extra=managed_fixtures + ('-O2',))

t.valid('recursive_owning_stress', '', text=r'''
enum List { Nil, Cons(i64, own<List>) }
extern fn on1x_unique_live_allocations()->i64;
extern fn on1x_test_unique_allocator_live()->i64;
fn empty()->own<List>{return box(variant<List>(Nil,record{}));}
fn prepend(item:i64,tail:own<List>)->own<List>{
  return box(variant<List>(Cons,(item,move(tail))));
}
fn main(){
  {
    var values=empty();
    for i in 0..50000 { values=prepend(i,move(values)); }
    assert(on1x_unique_live_allocations()==50001);
    assert(on1x_test_unique_allocator_live()==50001);
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

print('recursive owning ADTs: moves, cleanup, ABI, modules, failures and boundaries PASS')
