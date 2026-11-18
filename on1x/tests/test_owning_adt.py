#!/usr/bin/env python3
"""Nominal shared ownership of immutable S-expression nodes."""
import os
from support import NativeTests, run

t = NativeTests()
allocators = ('-include', t.root / 'tests/fixtures/closure_allocator.h',
              t.root / 'tests/fixtures/closure_allocator.c')
t.source('owning_adt_library', '''
record Cell<T> { item:T }
enum Choice<T> { Empty, Full(i64, Cell<T>) }
fn make(s:sexpr)->Choice<sexpr>{
  return variant<Choice<sexpr>>(Full,(42,record<Cell<sexpr>>{item:s}));
}
''')
t.valid('owning_adt_local', '42\n3\npayload\n', text='''
from owning_adt_library import Cell, Choice, make;
enum Outcome<T,E> { Ok(T), Err(E) }
extern fn live_environments()->i64;
fn use(s:sexpr) { println(sx_text(s)); }
fn save(n:i64)->fn()->sexpr {
  let item = make(sx_string("payload"));
  return fn()->sexpr {
    return match item with { Empty()=>sx_nil(), Full(_,record{item:text})=>text };
  };
}
fn result(ok:bool)->Outcome<Choice<sexpr>,sexpr> {
  if ok { return variant<Outcome<_,_>>(Ok,make(sx_symbol("ok"))); }
  return variant<Outcome<_,_>>(Err,sx_symbol("error"));
}
fn propagate(ok:bool)->Outcome<Choice<sexpr>,sexpr> {
  let value = result(ok)?;
  return variant<Outcome<_,_>>(Ok,value);
}
fn main() {
  make(sx_string("discarded"));
  match make(sx_string("match discard")) with {
    Empty()=>sx_nil(), Full(_,cell)=>cell["item"]
  };
  assert(data_live_nodes() == 0);
  { let f = save(0); defer use(f()); assert(data_live_nodes() == 1); }
  assert(data_live_nodes() == 1); # deferred function owns the remaining atom
  fn(){
    var active = make(sx_string("obsolete"));
    active = active;
    active = make(sx_symbol("present"));
    let picked = loop { break with active; };
    active = variant<Choice<sexpr>>(Empty, record{});
    println(match picked with { Empty()=>0, Full(n,c)=>{assert(sx_text(c["item"])=="present");n} });
    let err = propagate(false);
    println(match err with { Ok(_)=>0, Err(s)=>{assert(sx_text(s)=="error");3} });
    let ok = propagate(true);
    assert(match ok with { Ok(Full(n,_))=>n==42, _=>false });
  }();
  assert(data_live_nodes() == 1);
  assert(live_environments() == 3); # atom + defer record + defer capture
}
''', extra=allocators + ('-O2',))

# Scope exit drops the managed aggregate; releasing the node cascades through
# a large immutable list using the existing iterative RC runtime, not C recursion.
t.valid('owning_adt_stress', '', text='''
record Owned{entries:sexpr}
fn main(){
  {
    var r=record<Owned>{entries:sx_nil()};
    for i in 0..50000 { r=record<Owned>{entries:sx_cons(sx_nil(),r["entries"])}; }
    assert(data_live_nodes()==50000);
  }
  assert(data_live_nodes()==0);
}
''', extra=allocators + ('-O2',))

failure = t.build('owning_adt_failure', text='''
record Owned{entries:sexpr}
fn main(){let record=record<Owned>{entries:sx_string("allocation failure")};}
''', extra=allocators)
p = run([failure], env={**os.environ, 'ON1X_TEST_ENV_FAIL_AFTER': '0'})
assert p.returncode == 72 and 'environment allocation failure' in p.stderr, p.stderr

for name, text, expected in (
    ('unique', 'record R{x:json}fn main(){}', 'trivially copied'),
    ('view', 'enum R{X(json_view)}fn main(){}', 'trivially copied'),
    ('mutable', 'record R{x:buffer}fn main(){}', 'trivially copied'),
    ('closure', 'record R{x:fn()->i64}fn main(){}', 'trivially copied'),
    ('structural', 'fn main(){let x=record{x:sx_nil()};}', 'trivially copied'),
    ('arrays', 'record R{x:sexpr}fn main(){let x=array[record<R>{x:sx_nil()}];}', 'trivially copied'),
    ('identity', 'record R{x:sexpr}record S{x:sexpr}fn main(){let x:S=record<R>{x:sx_nil()};}', 'nominal'),
    ('recursive', 'enum R{End, Next(sexpr,R)}fn main(){}', 'recursive nominal'),
    ('extern', 'record R{x:sexpr}extern fn f(x:R);fn main(){}', 'extern ABI'),
):
    p = run([t.compiler, '--check', t.source('owning_adt_' + name, text)])
    assert p.returncode == 1 and expected in p.stderr and not p.stdout, (name, p.stderr)

mir_only = t.source('owning_adt_mir', '''
record Owned{entries:sexpr}
fn make()->Owned{return record<Owned>{entries:sx_nil()};}
fn main(){make();}
''')
p = run([t.compiler, '--emit-ir', mir_only])
assert p.returncode == 1 and 'ON1X-MIR-UNSUPPORTED' in p.stderr and not p.stdout, p.stderr
print('nominal sexpr owners: copy/drop, projections, matches, Result, defer, iterative release and failure PASS')
