#!/usr/bin/env python3
"""Checked lexical/shared/exclusive borrows through source and strict C11."""
import os
import shutil

from support import NativeTests, run

t = NativeTests()
sample = t.source('core_ref_example', '''
fn foo(k : ref i64) -> _ { k := 2; }
fn main() { let a = 4; foo(a); println(a); }
''')
t.valid('core_ref_example', '2\n', source=sample)
p = run([t.compiler, '--types', sample])
assert p.returncode == 0 and 'foo : fn(ref i64) -> void\n' in p.stdout, p.stderr

t.valid('core_ref_order', '10\n14\n23\n3\n10\n8\n8\n', text='''
fn mark(k: ref i64, delta: i64) -> i64 { k := k + delta; return k; }
fn update(before:i64, x:ref i64, side:i64, y:ref i64) {
  println(before); x := x + side; y := y + x;
}
fn recurse(k: ref i64, count: i64) {
  if count == 0 { return; }
  k := k + 1; recurse(k, count - 1);
}
fn main() {
  let a = 10; var b = 2; let c = 1;
  update(a, a, mark(c, 3), b); # a is read before borrowing it; c is distinct.
  println(a); println(b + 7);
  { let a = 0; recurse(a, 3); println(a); }
  let copy = a; mark(a, -4); println(a); assert(copy == 14);
  let k = 7; let snapshot = fn()->i64 { return k; };
  assert(mark(k, 1) == 8); println(k); assert(snapshot() == 7);
  let result = match k with { _ => { mark(k, 0); k } };
  println(result);
}
''', extra=('-O2',))

t.source('core_ref_lib', '''
fn clear(x: ref i64) { x := 0; }
fn peek(x: borrow i64)->i64 { return x; }
fn fold(x: ref i64) -> i64 {
  for (var i = 1; i < 4; x := x + 1) { i = i + 1; }
  return match x with { 7 => { x := x + 1; x }, _ => -1 };
}
''')
t.valid('core_ref_import', '8\n0\n', text='''
from core_ref_lib import clear as reset, fold, peek;
macro clear_here { rule($a:Expression) => expression { reset($a) }; }
fn main() {
 let a = 4;
 assert(peek(a)==4);
 println(fold(a)); clear_here!(a); println(a);
}
''')

t.valid('core_ref_copy', '9\n9\n', text='''
fn from_ref(x:ref i64)->fn()->i64 {
  let copy = x;
  x := 0;
  return fn()->i64 { return copy; };
}
fn twin(x:ref i64,y:ref i64) { x := y; }
fn unused(x:ref i64) {}
fn main() {
  let a=9; let b=a;
  let keep = from_ref(a);
  assert(a==0); println(keep());
  twin(a,b); println(a); assert(b==9);
}
''')

t.valid('core_ref_specialization', 'true\n42\n2\n', text='''
fn copy_while_updating(x:ref i64, y) -> _ { x:=2; return y; }
fn main(){
  let value=9;
  println(copy_while_updating(value,true));
  println(copy_while_updating(value,42));
  println(value);
}
''')

t.valid('core_borrow_referent_specialization', '4\nfalse\ntrue\n', text='''
fn copy(x:borrow _)->_ { return x; }
fn replace(x:ref _, value) { x:=value; }
fn main(){
  let number=4; println(copy(number));
  let flag=false; println(copy(flag));
  replace(flag,true); println(flag);
}
''')

t.valid('core_borrow_scalars', '4\n9\ntrue\nhi\n', text='''
fn read(x:borrow i64)->i64 { return x; }
fn flip(x:ref bool) { x:=true; }
fn show(x:borrow string) { println(x); }
fn main() {
  let a=4;
  {
    let shared=borrow a;
    println(read(shared));
  }
  {
    let unique=borrow mut a;
    {
      let snapshot=borrow unique;
      {
        let nested_snapshot=borrow snapshot;
        assert(nested_snapshot==4);
      }
      assert(snapshot==4);
    }
    unique:=8;
    {
      let nested=borrow mut unique;
      nested:=9;
    }
    assert(unique==9);
  }
  println(a);
  let flag=false; flip(flag); println(flag);
  let text="hi"; let view=borrow text; show(view);
}
''')

t.valid('core_borrow_lexical_identity', '1\n1\n1\n', text='''
fn main(){
  let value=1;
  {
    let value=borrow value;
    println(value);
  }
  println(value);
  let unique=borrow mut value;
  let first=borrow unique;
  let second=borrow first;
  let third=borrow second;
  let fourth=borrow third;
  println(fourth);
}
''')

t.valid('core_borrow_joins', '5\n5\n5\n8\n', text='''
fn main() {
  let value=5;
  if true {
    let left=borrow value;
    println(left);
  } else {
    let right=borrow value;
    println(right);
  }
  var count=0;
  while count<2 {
    let iteration=borrow value;
    println(iteration);
    count=count+1;
  }
  {
    let update=borrow mut value;
    update:=8;
  }
  println(value);
}
''')

t.valid('core_borrow_data', '2\n4\n7\n', text='''
fn main(){
  let values=array[1,2];
  { let view=borrow values; println(view[1]); }
  { let edit=borrow mut values; edit:=array[3,4]; }
  println(values[1]);
  let point=record{x:7,y:9};
  { let view=borrow point; println(view["x"]); }
}
''')

owner_source = t.source('core_borrow_owner', '''
record Cell { enabled:bool, value:i64 }
fn inspect(cell:borrow Cell)->i64 { return cell["value"]; }
fn update(cell:ref Cell, amount:i64) {
  cell:=record<Cell>{enabled:true,value:cell["value"]+amount};
}
fn snapshot(cell:borrow Cell)->Cell { return cell; }
extern fn on1x_unique_live_allocations()->i64;
fn main() {
  let owner=box(record<Cell>{enabled:false,value:3});
  assert(on1x_unique_live_allocations()==1);
  println(inspect(owner));
  update(owner,2);
  {
    let root=borrow mut owner;
    {
      let child=borrow root;
      println(child["value"]);
    }
    root:=record<Cell>{enabled:true,value:8};
    {
      let child=borrow mut root;
      child:=record<Cell>{enabled:true,value:9};
    }
  }
  let copied=snapshot(owner);
  drop(owner);
  assert(on1x_unique_live_allocations()==0);
  println(copied["value"]);
  println(copied["enabled"]);
}
''')
t.valid('core_borrow_owner', '3\n5\n9\ntrue\n', source=owner_source)
types = run([t.compiler, '--types', owner_source])
assert types.returncode == 0 and \
       'inspect : fn(borrow Cell) -> i64\n' in types.stdout and \
       'update : fn(ref Cell, i64) -> void\n' in types.stdout, types.stderr

owner_cleanup_source = t.source('core_borrow_owner_cleanup', '''
enum Node { Leaf(i64) }
record Holder { child:own<Node>, version:i64 }
fn replace(holder:ref Holder, child:own<Node>) {
  holder:=record<Holder>{
    child:move(child),
    version:holder["version"]+1
  };
}
extern fn on1x_unique_live_allocations()->i64;
fn main(){
  let holder=box(record<Holder>{
    child:box(variant<Node>(Leaf,1)),
    version:0
  });
  assert(on1x_unique_live_allocations()==2);
  let next=box(variant<Node>(Leaf,2));
  assert(on1x_unique_live_allocations()==3);
  replace(holder,move(next));
  assert(on1x_unique_live_allocations()==2);
  { let view=borrow holder; println(view["version"]); }
  drop(holder);
  assert(on1x_unique_live_allocations()==0);
}
''')
t.valid('core_borrow_owner_cleanup', '1\n', source=owner_cleanup_source)

cases = [
    ('temporary', 'fn f(k:ref i64){} fn main(){f(2);}', 'named addressable local'),
    ('arithmetic', 'fn f(k:ref i64){} fn main(){let a=1;f(a+1);}', 'named addressable local'),
    ('projection', 'fn f(k:ref i64){} fn main(){let a=array[1];f(a[0]);}', 'named addressable local'),
    ('field_origin', '''fn f(k:borrow i64){} fn main(){
       let point=record{x:1};f(point["x"]);}''', 'named addressable local'),
    ('duplicate', 'fn f(x:ref i64,y:ref i64){} fn main(){let a=4;f(a,a);}', 'overlapping exclusive'),
    ('later_read', 'fn f(x:ref i64,y:i64){} fn main(){let a=4;f(a,a);}', 'during its exclusive'),
    ('later_write', '''fn f(x:ref i64,y:i64){} fn main(){
       var a=4; f(a, match 1 with {_=>{a=1; 2}}); }''', 'during its exclusive'),
    ('later_capture', '''fn f(x:ref i64,y:fn()->i64){} fn main(){
       let a=4; f(a, fn()->i64 {return a;}); }''', 'during its exclusive'),
    ('inner_alias', '''fn g(x:ref i64)->i64{return x;} fn f(x:ref i64,y:i64){}
       fn main(){let a=4; f(a,g(a));}''', 'overlapping exclusive'),
    ('forwarded_alias', '''fn g(x:ref i64,y:ref i64){} fn f(x:ref i64){g(x,x);}
       fn main(){let a=4;f(a);}''', 'overlapping exclusive'),
    ('capture', '''fn f(x:ref i64)->fn()->i64{return fn()->i64{return x;};}
       fn main(){let a=4;f(a);}''', 'cannot capture borrowed place'),
    ('lexical_capture', '''fn main(){let a=4;let r=borrow a;
       let saved=fn()->i64{return r;};println(saved());}''',
     'cannot capture borrowed place'),
    ('return_borrow_expression', '''fn bad()->i64{let a=1;
       return borrow a;}fn main(){}''', 'direct immutable let initializer'),
    ('aggregate_borrow_expression', '''fn main(){let a=1;
       let pair=(borrow a,2);println(pair);}''',
     'direct immutable let initializer'),
    ('global_borrow_expression', '''fn target(){}fn main(){
       let view=borrow target;}''', 'named local place'),
    ('borrow_capture', '''fn f(x:ref i64){x:=2;} fn main(){
       let a=4; let saved=fn(){f(a);}; saved();}''', 'named addressable local'),
    ('defer', 'fn f(x:ref i64){} fn main(){let a=4;defer f(a);}', 'defer cannot retain'),
    ('function_value', 'fn f(x:ref i64){} fn main(){let saved=f;}', 'direct named call'),
    ('function_arg', 'fn f(x:ref i64){} fn sink(k:fn(i64)->void){} fn main(){sink(f);}', 'direct named call'),
    ('ref_local', 'fn main(){let a:ref i64=1;}', 'named On1x function parameter'),
    ('ref_return', 'fn f(x:ref i64)->ref i64{return x;} fn main(){}', 'named On1x function parameter'),
    ('ref_nested', 'fn f(x:fn(ref i64)->void){} fn main(){}', 'named On1x function parameter'),
    ('ref_element', 'record A{a:ref i64} fn main(){}', 'named On1x function parameter'),
    ('ref_extern', 'extern fn f(k:ref i64); fn main(){}', 'non-extern On1x function'),
    ('ref_lambda', 'fn main(){let f=fn(x:ref i64){};}', 'non-extern On1x function'),
    ('ref_pattern', 'fn f((x,y):ref i64){} fn main(){}', 'simple parameter'),
    ('store_local', 'fn main(){var x=4; x:=2;}', 'exclusive borrowed place'),
    ('ordinary_assignment', 'fn f(x:ref i64){x=2;} fn main(){}', "with ':='"),
    ('shared_store', 'fn f(x:borrow i64){x:=2;} fn main(){}',
     'exclusive borrowed place'),
    ('shared_then_exclusive', '''fn main(){let a=1;let r=borrow a;
       let m=borrow mut a;println(r);println(m);}''',
     'overlapping exclusive borrow'),
    ('exclusive_then_shared', '''fn main(){let a=1;let m=borrow mut a;
       let r=borrow a;println(m);println(r);}''',
     'conflicting shared borrow'),
    ('exclusive_root_read', '''fn main(){let a=1;let m=borrow mut a;
       println(a);println(m);}''', 'during its exclusive borrow'),
    ('shared_root_write', '''fn main(){var a=1;let r=borrow a;
       a=2;println(r);}''', 'during its shared borrow'),
    ('parent_while_reborrowed', '''fn main(){let a=1;let m=borrow mut a;
       {let child=borrow m;m:=2;println(child);}}''',
     'during its shared borrow'),
    ('borrowed_owner_drop', '''record R{value:i64}fn main(){
       let owner=box(record<R>{value:1});let view=borrow owner;
       drop(owner);println(view["value"]);}''', 'cannot consume'),
    ('borrowed_owner_move', '''record R{value:i64}
       fn sink(x:own<R>){drop(x);}fn main(){
       let owner=box(record<R>{value:1});let view=borrow owner;
       sink(move(owner));println(view["value"]);}''', 'cannot consume'),
    ('borrow_after_owner_move', '''record R{value:i64}fn main(){
       let owner=box(record<R>{value:1});drop(owner);
       let view=borrow owner;println(view["value"]);}''', 'borrow of moved'),
    ('move_borrowed_payload', '''enum N{Z}record R{child:own<N>}
       fn escape(value:borrow R)->R{return value;}fn main(){}''',
     'copy of unique local'),
    ('borrow_function_callback', '''fn view(x:borrow i64){}
       fn sink(f:fn(i64)->void){}fn main(){sink(view);}''',
     'direct named call'),
    ('shared_defer', '''fn view(x:borrow i64){}fn main(){
       let a=1;defer view(a);}''', 'defer cannot retain'),
    ('borrow_owner_handle', '''record R{value:i64}
       fn bad(x:borrow own<R>){}fn main(){}''',
     'names the referent type, not own'),
    ('lexical_concurrency', '''fn main(){let a=1;let r=borrow a;
       taskgroup{}println(r);}''', 'suspension/task boundaries'),
    ('concurrency', 'fn f(x:ref i64){} fn main(){taskgroup {}}', 'cannot be combined with cooperative concurrency'),
]
for name, source, expected in cases:
    src = t.source('core_ref_reject_' + name, source)
    out = t.work / 'rejected.c'
    out.write_text('untouched\n')
    p = run([t.compiler, '--emit-c', src, '-o', out])
    assert p.returncode == 1 and expected in p.stderr, (name, p.returncode, p.stderr)
    assert str(src) + ':' in p.stderr and not p.stdout, (name, p.stderr)
    assert out.read_text() == 'untouched\n'

for mode, expected in (
    ('--emit-ir', 'ON1X-MIR-UNSUPPORTED'),
    ('--emit-ir-c', 'ON1X-MIR-UNSUPPORTED'),
    ('--emit-ast', 'not part of PPX Unit v1'),
):
    p = run([t.compiler, mode, sample])
    assert p.returncode == 1 and expected in p.stderr and not p.stdout, (mode, p.stderr)

lexical_source = t.work / 'core_borrow_joins.on1x'
for mode, expected in (
        ('--emit-ir', 'ON1X-MIR-UNSUPPORTED'),
        ('--emit-ir-c', 'ON1X-MIR-UNSUPPORTED'),
        ('--emit-ast', 'lexical borrows are not PPX Unit v1 wire data')):
    p = run([t.compiler, mode, lexical_source])
    assert p.returncode == 1 and expected in p.stderr and not p.stdout, \
        (mode, p.stderr)

# Optional independent adapters must either be unavailable or reject the loan
# metadata they do not model. They may not report success over erased referents.
for mode, expected, unavailable in (
        ('--check-memory', 'expression memory model unavailable',
         'memory checking unavailable'),
        ('--check-types', 'outside the structural adapter',
         'structural type checking unavailable')):
    p = run([t.compiler, mode, lexical_source])
    if p.returncode == 2:
        assert unavailable in p.stderr and not p.stdout, (mode, p.stderr)
    else:
        assert p.returncode == 1 and expected in p.stderr and not p.stdout, \
            (mode, p.stderr)

# Generated owner-payload borrows remain warning-clean strict C11 with both
# available production compilers. The same executable is also exercised under
# ASan+UBSan so pointer aliasing and owner cleanup are checked dynamically.
generated = t.work / 'core_borrow_owner.c'
generated_text = generated.read_text(encoding='utf-8')
assert 'on1x_unique_borrow(' in generated_text and ' const *' in generated_text
seen = set()
for compiler_name in ('gcc', 'clang'):
    compiler = shutil.which(compiler_name)
    if not compiler or os.path.realpath(compiler) in seen:
        continue
    seen.add(os.path.realpath(compiler))
    executable = t.work / ('core-borrow-' + compiler_name)
    compiled = run([
        compiler, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-I', t.root / 'runtime', generated, t.root / 'runtime/on1x_runtime.c',
        '-o', executable])
    assert compiled.returncode == 0, (compiler_name, compiled.stderr)
    executed = run([executable])
    assert executed.returncode == 0 and executed.stdout == '3\n5\n9\ntrue\n' \
        and not executed.stderr, (compiler_name, executed)

sanitized_generated = t.work / 'core_borrow_owner_cleanup.c'
sanitized = t.work / 'core-borrow-sanitized'
compiled = run([
    t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
    '-fno-omit-frame-pointer', '-I', t.root / 'runtime', sanitized_generated,
    t.root / 'runtime/on1x_runtime.c', '-fsanitize=address,undefined',
    '-o', sanitized])
assert compiled.returncode == 0, compiled.stderr
executed = run(
    [sanitized],
    env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=1:abort_on_error=1',
         'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'})
assert executed.returncode == 0 and executed.stdout == '1\n' and \
       not executed.stderr, executed

# The opaque unique-owner accessor fails closed for null, taken and
# size-mismatched direct runtime callers.
abi_source = t.work / 'core-borrow-abi.c'
abi_source.write_text(r'''
#include <on1x_runtime.h>
#include <string.h>
typedef struct { int64_t value; } payload;
static void destroy(void *raw){memset(raw,0,sizeof(payload));}
int main(int argc,char **argv){
  payload source={7},taken={0};
  on1x_unique *owner;
  if(argc!=2)return 3;
  if(argv[1][0]=='n')
    (void)on1x_unique_borrow(NULL,sizeof(payload),"borrow-abi",7,9);
  owner=on1x_unique_try_box(&source,sizeof(source),destroy);
  if(!owner)return 4;
  if(argv[1][0]=='s')
    (void)on1x_unique_borrow(owner,sizeof(payload)+1,"borrow-abi",7,9);
  if(argv[1][0]=='t'){
    on1x_unique_take(&owner,&taken,sizeof(taken),"borrow-abi",6,1);
    (void)on1x_unique_borrow(owner,sizeof(payload),"borrow-abi",7,9);
  }
  payload *view=(payload *)on1x_unique_borrow(
      owner,sizeof(payload),"borrow-abi",7,9);
  if(view->value!=7)return 5;
  on1x_unique_drop(owner);
  return 0;
}
''', encoding='utf-8')
abi_executable = t.work / 'core-borrow-abi'
compiled = run([
    t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
    '-I', t.root / 'runtime', abi_source, t.root / 'runtime/on1x_runtime.c',
    *t.flags, '-o', abi_executable])
assert compiled.returncode == 0, compiled.stderr
healthy = run([abi_executable, 'ok'])
assert healthy.returncode == 0 and not healthy.stdout and not healthy.stderr, healthy
for mode in ('null', 'size', 'taken'):
    failed = run(
        [abi_executable, mode],
        env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
    assert failed.returncode == 72 and not failed.stdout and \
           'borrow-abi:7:9: On1x ownership error: invalid borrow from owning box' \
           in failed.stderr, (mode, failed)

# Deliberately exceed the separate canonical loan-work budget. This must reject
# deterministically rather than becoming quadratic unbounded work.
budget_source = t.source(
    'core_borrow_budget',
    'fn main(){let root=1;\n' +
    ''.join(f'let view{i}=borrow root;\n' for i in range(2900)) +
    'println(root);}\n')
budget = run([t.compiler, '--emit-c', budget_source])
assert budget.returncode == 1 and not budget.stdout and \
       'borrow analysis work budget exhausted' in budget.stderr and \
       str(budget_source) + ':' in budget.stderr, budget

print('borrows: shared/exclusive lexical loans, owners, joins, faults, sanitizers and fail-closed boundaries PASS')
