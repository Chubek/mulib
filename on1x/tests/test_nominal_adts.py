#!/usr/bin/env python3
"""Native nominal declarations: executable constructors, inference and identity."""
import json
import os
import random
import re
from support import NativeTests, run

t = NativeTests()
t.valid('nominal_adts', '42\ntrue\n-7\n')
t.valid('nominal_contextual_identifiers','true\ntrue\n',text='''
fn main(){let record=1;let variant=2;let other=3;
 println(record<other);println(variant<other);
 assert(record<variant&&variant<other);
 if record<other {assert(true);} if variant<other {assert(true);}}
''')

t.valid('nominal_order', '1\n2\n3\n4\n42\n', text='''
record Point { x:i64, y:i64 }
enum Pair<T> { Empty, Two(T,T) }
fn mark(n:i64)->i64{println(n);return n;}
fn main(){let p=record<Point>{y:mark(1),x:mark(2)};
 let v=variant<Pair<i64>>(Two,(mark(3),mark(4)));
 assert(p["x"]==2);assert(p["y"]==1);
 println(match v with {Empty()=>0,Two(a,b)=>a+b+35});}
''')

t.valid('nominal_nested', '42\n9\n', text='''
record Box<T> { value:T }
enum Option<T> { None,Some(T) }
fn identity(x)->_{return x;}
fn main(){
 let b=identity(record<Box<Option<i64>>>{value:variant<Option<_>>(Some,42)});
 let list=array[b,b];let s=list[1:2];
 println(match s[0]["value"] with {None()=>0,Some(n)=>n});
 let r=loop{break with record<Box<_>>{value:9};};println(r["value"]);
}
''')

t.valid('nominal_generic', '42\ntrue\n8\n', text='''
record Box<T> {value:T}
enum Option<T>{None,Some(T)}
fn wrap(x)->_{return record<Box<_>>{value:x};}
fn unwrap(x:Box<_>)->_{return x["value"];}
fn main(){println(unwrap(wrap(42)));println(unwrap(wrap(true)));
 let id:forall(T) fn(Option<T>)->Option<T>=fn(x)->_{return x;};
 let v=id(variant<Option<i64>>(Some,8));
 println(match v with {None()=>0,Some(n)=>n});}
''')

t.valid('nominal_hygiene_staging', '42\n7\n', text='''
record Box<T>{value:T}
enum Maybe<T>{No,Yes(T)}
macro make{rule($n:Expression)=>expression{record<Box<_>>{value:$n}};}
fn main(){let Box=90;let b=make!(42);assert(Box==90);println(b["value"]);
 let code=quote{variant<Maybe<i64>>(Yes,7)};
 println(match run(code) with {No()=>0,Yes(n)=>n});}
''')

# Constructor catalogs, malformed declarations, nominal isolation and resources.
cases = [
 ('record_identity','record A{x:i64} record B{x:i64} fn main(){let b:B=record<A>{x:1};}', 'nominal'),
 ('record_structural','record A{x:i64} fn main(){let b:record{x:i64}=record<A>{x:1};}', 'nominal'),
 ('enum_identity','enum A{X(i64)} enum B{X(i64)} fn main(){let b:B=variant<A>(X,1);}', 'nominal'),
 ('phantom_identity','enum Tag<T>{Empty} fn main(){let a:Tag<bool>=variant<Tag<i64>>(Empty,record{});}', 'expected bool'),
 ('phantom_managed','enum Tag<T>{Empty} fn main(){let a=variant<Tag<buffer>>(Empty,record{});}', 'unsupported buffer'),
 ('arity','enum Option<T>{None,Some(T)} fn main(){let a:Option=variant<Option<i64>>(Some,1);}', 'arity'),
 ('many_args','record A{x:i64} fn main(){let a=record<A<i64>>{x:1};}', 'arity'),
 ('unknown','record A{x:Missing} fn main(){}', 'unknown type'),
 ('managed','record A{x:buffer} fn main(){}', 'unsupported buffer'),
 ('unique','enum A{X(json)} fn main(){}', 'unsupported json'),
 ('view','record A{x:json_view} fn main(){}', 'unsupported json_view'),
 ('closure_resource','record A{x:fn()->json} fn main(){}', 'unsupported json'),
 ('closure_cycle','record A{x:fn(A)->void} fn main(){}', 'recursive'),
 ('hole_field','record A{x:_} fn main(){}', 'unsupported _'),
 ('direct_cycle','enum A{Next(A)} fn main(){}', 'recursive'),
 ('mutual_cycle','record A{x:B} record B{x:A} fn main(){}', 'recursive'),
 ('boxed_cycle','enum A{Next(box<A>)} fn main(){}', 'unknown type'),
 ('duplicate','enum A{X} enum A{Y} fn main(){}', 'duplicate'),
 ('duplicate_parameter','enum A<T,T>{X(T)} fn main(){}', 'duplicate'),
 ('duplicate_field','record A{x:i64,x:bool} fn main(){}', 'duplicate'),
 ('empty_enum','enum A{} fn main(){}', 'at least one'),
 ('unknown_constructor','enum A{X(i64)} fn main(){let a=variant<A>(Y,1);}', 'unknown sum constructor'),
 ('wrong_payload','enum A{X(i64)} fn main(){let a=variant<A>(X,true);}', 'expected i64'),
 ('wrong_record','enum A{X(i64)} fn main(){let a=record<A>{X:1};}', 'declared record'),
 ('wrong_variant','record A{x:i64} fn main(){let a=variant<A>(x,1);}', 'closed sum'),
 ('missing_field','record A{x:i64,y:i64} fn main(){let a=record<A>{x:1};}', 'data labels'),
 ('non_exhaustive','enum A{X,Y(i64)} fn main(){let a=variant<A>(X,record{});println(match a with {X()=>0});}', 'non-exhaustive'),
 ('redundant','enum A{X,Y(i64)} fn main(){let a=variant<A>(X,record{});println(match a with {_=>0,Y(_)=>1});}', 'redundant'),
 ('arithmetic','record A{x:i64} fn main(){let a=record<A>{x:1};println(a+a);}', 'numeric'),
 ('extern','record A{x:i64} extern fn foreign(a:A)->i64; fn main(){println(foreign(record<A>{x:1}));}', 'extern'),
]
for name, source, expected in cases:
    path=t.source('nominal_reject_'+name,source)
    output=t.work/(name+'.rejected.c')
    output.write_text('preserved output\n')
    p=run([t.compiler,'--emit-c',path,'-o',output])
    assert p.returncode==1 and expected in p.stderr, (name,p.returncode,p.stderr)
    assert ':1:' in p.stderr and not p.stdout, (name,p.stderr)
    assert output.read_text()=='preserved output\n'

# Canonical module identity survives aliases/re-exports; identical definitions in
# different modules stay distinct. Type names occupy a separate namespace.
t.source('nominal_library','''
record Box<T>{value:T}
enum Option<T>{None,Some(T)}
fn wrap(x)->_{return record<Box<_>>{value:x};}
fn choose(x:Option<_>,fallback:_)->_{return match x with{None()=>fallback,Some(n)=>n};}
''')
t.source('nominal_reexport','from nominal_library import Box as Cell, Option as Maybe, wrap, choose;')
t.valid('nominal_import','42\n9\n',text='''
import nominal_library as L;
from nominal_reexport import Cell,Maybe,wrap,choose;
fn take(x:L.Box<i64>)->i64{return x["value"];}
fn main(){let x:Cell<i64>=wrap(42);println(take(x));
 let y:Maybe<i64>=variant<L.Option<_>>(Some,9);println(choose(y,0));}
''')
t.source('nominal_other','record Box<T>{value:T}')
p=run([t.compiler,'--check',t.source('nominal_modules_distinct','''
import nominal_library as A;import nominal_other as B;
fn main(){let b:B.Box<i64>=A.wrap(1);}
''')])
assert p.returncode==1 and 'nominal' in p.stderr,p.stderr

# Unimported annotations cannot resolve to entry-module types after flattening.
t.source('nominal_hidden','fn hidden(x:Secret)->i64{return x["n"];}')
p=run([t.compiler,'--check',t.source('nominal_unimported','''
import nominal_hidden as H;record Secret{n:i64}fn main(){}
''')])
assert p.returncode==1 and 'not imported' in p.stderr,p.stderr
t.valid('nominal_record_pattern','42\n',text='''
record Pair<T>{first:T,second:T}
fn main(){let p=record<Pair<i64>>{first:40,second:2};
 println(match p with {record{first:x,second:y}=>x+y});}
''')

# Nominal values use the existing closure environment lifecycle, not new owners.
closure=t.valid('nominal_capture','42\n',text='''
record Box<T>{value:T}
extern fn live_environments()->i64;
fn make(x:Box<i64>)->fn()->Box<i64>{return fn()->Box<i64>{return x;};}
fn main(){{let f=make(record<Box<i64>>{value:42});let copy=f;
 let r=loop{break with copy();};println(r["value"]);}assert(live_environments()==0);}
''',extra=('-include',t.root/'tests/fixtures/closure_allocator.h',t.root/'tests/fixtures/closure_allocator.c'))
p=run([closure],env={**os.environ,'ON1X_TEST_ENV_FAIL_AFTER':'0'})
assert p.returncode==72 and 'allocation' in p.stderr,p.stderr

# Named void procedures and owned capturing environments can live inside
# polymorphic nominal records and the active payload of an enum.
t.valid('nominal_text_and_procedures', 'body\n40\nbody\n42\nbody\n',
        text=r'''
enum Outcome<T,E> { Ok(T), Err(E) }
enum Maybe<T> { Nothing, Something(T) }
record Document { body: string }
record Delivery<M> { doc: Document, meta: Maybe<M> }
record Receiver<C,M> { context: C, handle: fn(Delivery<M>, C) -> void }
extern fn live_environments() -> i64;
fn inspect<C,M>(self: borrow Receiver<C,M>, r: Delivery<M>) -> Outcome<Document,string> {
  self["handle"](r, self["context"]);
  return variant<Outcome<Document,string>>(Ok,r["doc"]);
}
fn named(r: Delivery<i64>, context: i64) {
  println(r["doc"]["body"]);
  println(context);
}
fn handler<M>(number: i64, example: M) -> fn(Delivery<M>, i64) -> void {
  return fn(r: Delivery<M>, context: i64) {
    println(r["doc"]["body"]);
    println(context + number);
  };
}
fn main() {
  let doc = record<Document>{body:text_concat("bo","dy")};
  let delivery = record<Delivery<i64>>{
    doc:doc, meta:variant<Maybe<i64>>(Something,1)};
  {
    let receiver = record<Receiver<i64,i64>>{context:40,handle:named};
    let first = inspect(receiver,delivery);
    let capturing = record<Receiver<i64,i64>>{context:22,handle:handler(20,1)};
    let wrapped = variant<Maybe<Receiver<i64,i64>>>(Something,capturing);
    assert(live_environments()==1);
    match wrapped with {
      Nothing() => assert(false),
      Something(item) => {
        let second = inspect(item,delivery);
        match second with { Ok(result) => println(result["body"]),
                            Err(message) => println(message) };
      }
    };
    match first with { Ok(result) => assert(result["body"]=="body"),
                       Err(message) => assert(false) };
  }
  assert(live_environments()==0);
}
''',extra=('-include',t.root/'tests/fixtures/closure_allocator.h',t.root/'tests/fixtures/closure_allocator.c'))

# Real aggregate calls across C translation units sharing one generated interface;
# not separate On1x compilation or aggregate FFI admission. C++ calls it as well.
abi_source=t.source('nominal_cross_tu','''
record Packet{n:i64,ready:bool}
fn produce(n:i64)->Packet{return record<Packet>{n:n,ready:true};}
fn consume(p:Packet)->i64{if p["ready"]{return p["n"]+2;}return 0;}
fn main(){println(consume(produce(40)));}
''')
p=run([t.compiler,'--emit-c',abi_source]);assert p.returncode==0,p.stderr
generated=p.stdout
definition=re.search(r'(?m)^\w+ on1x_fn_produce\([^\n]*\) \{',generated)
assert definition
start=definition.start();end=definition.end();depth=1
while depth:
    if generated[end]=='{':depth+=1
    if generated[end]=='}':depth-=1
    end+=1
provider=t.work/'nominal_provider.c';caller=t.work/'nominal_caller.c'
header=t.work/'nominal_interface.h'
interface=generated[:start]
interface=re.sub(r'(?m)^static [^\n]+;\n','',interface)
interface=re.sub(r'(?ms)^static [^\n]+ \{\n.*?^\}\n','',interface)
header.write_text(interface)
provider.write_text(interface+generated[start:end]+'\n')
caller.write_text(generated[:start]+generated[end:])
exe=t.work/'nominal_separate_c'
strict=['-Wall','-Wextra','-Wpedantic','-Werror','-I',t.root/'runtime',*t.flags]
p=run([t.cc,'-std=c11',*strict,provider,caller,t.root/'runtime/on1x_runtime.c','-o',exe]);assert p.returncode==0,p.stderr
p=run([exe]);assert p.returncode==0 and p.stdout=='42\n',p.stderr
cpp=t.work/'nominal_consumer.cpp'
cpp.write_text('extern "C" {\n#include "nominal_interface.h"\n}\nint main(){return on1x_fn_consume(on1x_fn_produce(40))==42?0:1;}\n')
objects=[]
for file in (provider,caller,t.root/'runtime/on1x_runtime.c'):
    obj=t.work/(file.stem+'.o');objects.append(obj)
    p=run([t.cc,'-std=c11',*strict,'-Dmain=on1x_test_hidden_main','-c',file,'-o',obj]);assert p.returncode==0,p.stderr
obj=t.work/'nominal_consumer.o';objects.append(obj)
p=run([t.cc,'-x','c++','-std=c++20',*strict,'-c',cpp,'-o',obj]);assert p.returncode==0,p.stderr
exe=t.work/'nominal_separate_cpp'
p=run([t.cc,*t.flags,*objects,'-o',exe]);assert p.returncode==0,p.stderr
p=run([exe]);assert p.returncode==0,p.stderr

# Declaration/instantiation budgets include unused declarations.
for name,source in [
 ('wide','record Huge{x:array<array<i64,256>,256>}fn main(){}'),
 ('params','record Many<'+','.join('T'+str(i) for i in range(33))+'> {}fn main(){}'),
 ('depth','record D0{x:i64}'+''.join(f'record D{i}{{x:D{i-1}}}' for i in range(1,36))+'fn main(){}'),
]:
    p=run([t.compiler,'--emit-c',t.source('nominal_budget_'+name,source)])
    assert p.returncode==1 and ('limit' in p.stderr or 'budget' in p.stderr) and not p.stdout,(name,p.stderr)

# Differential generated values against a seeded Python model; no aggregate MIR
# claim. Optimized and unoptimized host C11 agree under the same runtime ABI.
rng=random.Random(20261117)
for seed in range(6):
    values=[rng.randint(-100,100) for _ in range(10)]
    program='''enum Result<T,E>{Ok(T),Err(E)}
fn score(v:Result<i64,i64>)->i64{return match v with{Ok(n)=>n,Err(e)=>0-e};}
fn main(){var total=0;
'''
    expected=0
    for i,n in enumerate(values):
        tag='Ok' if i%3 else 'Err'
        program+=f'total=total+score(variant<Result<_,_>>({tag},{n}));\n'
        expected+=n if tag=='Ok' else -n
    program+='println(total);}'
    t.valid('nominal_oracle_'+str(seed),str(expected)+'\n',text=program,extra=('-O2',))

source=t.root/'examples/nominal_adts.on1x'
first=run([t.compiler,'--emit-c',source]);second=run([t.compiler,'--emit-c',source])
assert first.returncode==second.returncode==0 and first.stdout==second.stdout
for mode in ('--emit-ir','--emit-ir-c'):
    p=run([t.compiler,mode,source])
    assert p.returncode==1 and 'ON1X-MIR-UNSUPPORTED' in p.stderr and not p.stdout,p.stderr

env=dict(os.environ,ON1X_PPX_SCHEMA=str(t.root/'grammar/PPX.schema.json'))
identity=t.compiler.parent/'stdppx/on1x-ppx-identity'
if identity.exists():
    out=t.work/'nominal_identity.c'
    p=run([t.compiler,'--ppx','identity',identity,'--emit-c',source,'-o',out],env=env)
    assert p.returncode==0,p.stderr
    assert out.read_text()==first.stdout

# Both derive implementations preserve the new declarations and only render
# named source signatures. This is not a typed ADT-derive facility.
derive_source=t.source('nominal_derive','''
enum Maybe<T>{No,Yes(T)}
@derive("derive","signature") fn read(x:Maybe<i64>)->i64{
 return match x with {No()=>0,Yes(n)=>n};}
fn main(){println(read_signature());println(read(variant<Maybe<i64>>(Yes,42)));}
''')
processors=[('python',['--stdppx','derive'])]
derive=t.compiler.parent/'stdppx/on1x-ppx-derive'
if derive.exists():processors.append(('native',['--ppx','derive',derive]))
for name,options in processors:
    out=t.work/('nominal_derive_'+name+'.c');exe=t.work/('nominal_derive_'+name)
    p=run([t.compiler,'--emit-c',derive_source,*options,'-o',out],env=env);assert p.returncode==0,p.stderr
    p=run([t.cc,'-std=c11',*strict,out,t.root/'runtime/on1x_runtime.c','-o',exe]);assert p.returncode==0,p.stderr
    p=run([exe]);assert p.returncode==0 and p.stdout=='fn(Maybe<i64>) -> i64\n42\n',p.stderr

assert json.loads((t.root/'grammar/PPX.schema.json').read_text())==json.loads((t.root/'stdppx/schemas/PPX.schema.json').read_text())
print('nominal records/enums: C11, specialization, hygiene/staging/imports, seeded oracles and rejection PASS')
