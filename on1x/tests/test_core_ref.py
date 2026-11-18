#!/usr/bin/env python3
"""Checked call-scoped ref i64 places through the source and C11 toolchains."""
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
fn fold(x: ref i64) -> i64 {
  for (var i = 1; i < 4; x := x + 1) { i = i + 1; }
  return match x with { 7 => { x := x + 1; x }, _ => -1 };
}
''')
t.valid('core_ref_import', '8\n0\n', text='''
from core_ref_lib import clear as reset, fold;
macro clear_here { rule($a:Expression) => expression { reset($a) }; }
fn main() { let a = 4; println(fold(a)); clear_here!(a); println(a); }
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

cases = [
    ('temporary', 'fn f(k:ref i64){} fn main(){f(2);}', 'named addressable local'),
    ('arithmetic', 'fn f(k:ref i64){} fn main(){let a=1;f(a+1);}', 'named addressable local'),
    ('projection', 'fn f(k:ref i64){} fn main(){let a=array[1];f(a[0]);}', 'named addressable local'),
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
       fn main(){let a=4;f(a);}''', 'cannot capture ref i64'),
    ('borrow_capture', '''fn f(x:ref i64){x:=2;} fn main(){
       let a=4; let saved=fn(){f(a);}; saved();}''', 'named addressable local'),
    ('defer', 'fn f(x:ref i64){} fn main(){let a=4;defer f(a);}', 'defer cannot retain'),
    ('function_value', 'fn f(x:ref i64){} fn main(){let saved=f;}', 'direct named call'),
    ('function_arg', 'fn f(x:ref i64){} fn sink(k:fn(i64)->void){} fn main(){sink(f);}', 'direct named call'),
    ('ref_local', 'fn main(){let a:ref i64=1;}', 'only a named On1x function parameter'),
    ('ref_return', 'fn f(x:ref i64)->ref i64{return x;} fn main(){}', 'only a named On1x function parameter'),
    ('ref_nested', 'fn f(x:fn(ref i64)->void){} fn main(){}', 'only a named On1x function parameter'),
    ('ref_element', 'record A{a:ref i64} fn main(){}', 'only a named On1x function parameter'),
    ('ref_extern', 'extern fn f(k:ref i64); fn main(){}', 'non-extern On1x function'),
    ('ref_lambda', 'fn main(){let f=fn(x:ref i64){};}', 'non-extern On1x function'),
    ('ref_pattern', 'fn f((x,y):ref i64){} fn main(){}', 'simple parameter'),
    ('ref_other', 'fn f(x:ref bool){} fn main(){}', 'only ref i64'),
    ('store_local', 'fn main(){var x=4; x:=2;}', 'requires a ref i64 parameter'),
    ('ordinary_assignment', 'fn f(x:ref i64){x=2;} fn main(){}', "with ':='"),
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

print('ref i64: C11 mutation, lexical places, ordering, reborrow, and fail-closed boundaries PASS')
