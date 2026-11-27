"""Dot projections, module qualification and static enum constructors."""
from support import NativeTests, run

t = NativeTests()

t.valid('dot_record', '42\n42\n42\n42\n', text='''
record Point { x: i64 }
record Box<T> { inner: T }
fn make()->Box<Point>{return record<Box<Point>>{inner:record<Point>{x:42}};}
macro project {rule ($value:Expression)=>expression {$value.inner.x};}
fn main(){
  let anon=record{x:42}; println(anon.x);
  let b=make(); assert(b.inner["x"]==42 && b["inner"].x==42);
  println(make().inner.x);
  println((b).inner.x);
  println(project!(array[b,b][1]));
}
''')

t.source('field_provider', '''
record Point { x: i64 }
enum Message<T> { Empty, PointValue(Point), Pair(T,T) }
fn point(n:i64)->Point{return record<Point>{x:n};}
macro number {rule()=>expression {42};}
''')
t.valid('dot_module', '42\n7\n42\n4\n42\n42\n', text='''
import field_provider as M;
import field_provider as print;
import std.Int;
fn read(v:M.Message<i64>)->i64{
  return match v with {Empty()=>0,PointValue(p)=>p.x,Pair(a,b)=>a+b};
}
fn main(){
  println(M.point(42).x);
  {let M=record{point:record{x:7}};println(M.point.x);}
  println(M.number!());
  println(std.Int.div(8,2));
  println(read(M.Message<i64>::PointValue(M.point(42))));
  println(print.point(42).x);
}
''')

t.valid('static_members', '42\n7\n42\n43\n42\n', text='''
record Box { value: i64 }
impl Box {
  value answer:i64=42;
  fn get(self)->i64{return self.value;}
}
enum Choice { Empty, Full(Box), Conflict }
impl Choice { value answer:i64=42; value Conflict:i64=43; }
fn main(){
  let b=record<Box>{value:7};
  println(Box::answer); println(b::get());
  println(Choice::answer);
  println(Choice::Conflict);
  let c=Choice::Full(b);
  println(match c with {Empty()=>0,Full(p)=>p.value+35,Conflict()=>0});
}
''')

enums = '''
enum Pair<T> { Empty, Two(T,T) }
fn total(p:Pair<i64>)->i64{return match p with {Empty()=>0,Two(a,b)=>a+b};}
fn mark(n:i64)->i64{println(n);return n;}
fn main(){
  let zero:Pair<i64>=Pair::Empty;
  println(total(zero));
  println(total(Pair<i64>::Empty()));
  println(total(Pair::Two(mark(40),mark(2))));
  println(total(Pair<i64>::Two((19,23))));
  println(total(variant<Pair<i64>>(Two,(39,3))));
}
'''
t.valid('static_variants', '0\n0\n40\n2\n42\n42\n42\n', text=enums)

# No new wire constructors: emit-ast and an identity PPX see the existing
# ValueData field and variant expressions.
ppx = t.source('field_ppx', '''
enum E<T> { None, Some(record{x:T}) }
macro wrap {rule($n:Expression)=>expression{E<i64>::Some(record{x:$n})};}
fn main(){let e=wrap!(42);println(match e with {None()=>0,Some(r)=>r.x});}
''')
p = run([t.compiler, '--emit-ast', ppx])
assert p.returncode == 0 and '(ValueData "field" "x"' in p.stdout, p.stderr
assert '(ValueData "variant" "Some"' in p.stdout, p.stdout
p = run([t.compiler, '--check', ppx, '--ppx', 'identity', t.root/'stdppx/identity.py'])
assert p.returncode == 0, p.stderr

for name, text, message in [
    ('missing_field', 'fn main(){let p=record{x:42};println(p.y);}', 'unknown record field'),
    ('static_instance_field', 'record P{x:i64} fn main(){println(P::x);}', 'has no inherent member'),
    ('dot_static_value', '''
       record P{x:i64} impl P { value answer:i64=42; }
       fn main(){let p=record<P>{x:0};println(p.answer);}
       ''', 'unknown record field'),
    ('bad_variant', 'enum E{X} fn main(){let e=E::Y;}', 'has no inherent member'),
    ('missing_payload', 'enum E{X(i64)} fn main(){let e=E::X;}', 'requires a payload'),
    ('bad_payload', 'enum E{X(i64)} fn main(){let e=E::X(true);}', 'expected i64'),
    ('bad_payload_arity', 'enum E<T>{X(T)} fn main(){let e=E::X(1,2);}', 'payload arity mismatch'),
    ('unsafe_payload', '''
       enum E{X(i64),Y} fn main(){let e=E::Y;println(e.X);}
       ''', 'field projection requires a known record shape'),
    ('unqualified_payload', '''
       enum E{X(i64),Y} fn main(){let e=E::Y;println(e::X(1));}
       ''', 'has no inherent member'),
]:
    p = run([t.compiler, '--check', t.source(name, text)])
    assert p.returncode == 1 and message in p.stderr, (name, p.stderr)
