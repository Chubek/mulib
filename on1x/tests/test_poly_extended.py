from support import NativeTests, run
import os

t = NativeTests()
t.valid('global_polymorphism', '42\ntrue\n42\ntrue\n')
t.valid('destructuring_polymorphism', '42\ntrue\n42\ntext\n')
t.valid('polymorphic_parameters', '42\ntrue\n42\ntrue\n')
t.valid('forward_global', '42\ntrue\n', text='''
fn forward(x) -> _ { return identity(x); }
fn main() { println(forward(42)); println(forward(true)); }
fn identity(x) -> _ { return x; }
''')
t.valid('recursive_global', '42\ntrue\n', text='''
fn repeat(x, count:i64) -> _ { if count == 0 { return x; } return repeat(x, count-1); }
fn main() { println(repeat(42, 2)); println(repeat(true, 3)); }
''')
t.valid('mutual_globals', '42\ntrue\n', text='''
fn even(x, n:i64) -> _ { if n==0 {return x;} return odd(x,n-1); }
fn odd(x, n:i64) -> _ { if n==0 {return x;} return even(x,n-1); }
fn main() { println(even(42,10)); println(odd(true,9)); }
''')
t.valid('overlapping_components', '42\ntrue\n', text='''
fn first(x,n:i64) -> _ { if n==0{return x;} if n==1{return second(x,0);} return third(x,n-1); }
fn second(x,n:i64) -> _ { return first(x,n); }
fn third(x,n:i64) -> _ { return second(x,n); }
fn main() { println(first(42,5)); println(third(true,3)); }
''')
t.source('generic_library', 'fn imported(x) -> _ { return x; }')
t.valid('generic_import', '42\ntrue\n', text='''
import "generic_library.on1x";
fn main() { let alias = imported; println(alias(42)); println(alias(true)); }
''')
t.valid('nested_pattern', '42\ntrue\ntext\nfalse\n', text='''
fn main() {
 let (id, (other, flag)): (fn(_) -> _, (fn(_) -> _, bool)) =
   (fn(x) -> _ { return x; }, (fn(y) -> _ { return y; }, false));
 println(id(42)); println(id(true)); println(other("text")); println(flag);
}
''')
t.valid('forall_forwarding', '42\ntrue\n', text='''
fn use(f: forall(a) fn(a) -> a) { println(f(42)); println(f(true)); }
fn forward(g: forall(b) fn(b) -> b) { let copy = g; use(copy); }
fn main() { let id = fn(x) -> _ { return x; }; forward(id); }
''')
t.valid('forall_return', '42\ntrue\n', text='''
fn make() -> forall(a) fn(a) -> a { return fn(x) -> _ { return x; }; }
fn main() { let id = make(); println(id(42)); println(id(true)); }
''')
t.valid('forall_lambda', '42\ntrue\n', text='''
fn main() {
 let consume = fn(f: forall(a) fn(a) -> a) { println(f(42)); println(f(true)); };
 consume(fn(x) -> _ { return x; });
}
''')
t.valid('forall_local_annotation', '42\ntrue\n', text='''
fn main() { let id: forall(a) fn(a) -> a = fn(x) -> _ { return x; };
 println(id(42)); println(id(true)); }
''')
t.valid('unused_forall_value', '', text='''
fn accept(f:forall(a)fn(a)->a) {}
fn main(){accept(fn(x)->_ {return x;});}
''')
t.valid('poly_param_explicit', '42\ntrue\n', text='''
fn use(f:forall(a)fn(a)->a) { let ordinary:fn(i64)->i64=f; println(ordinary(42)); println(f(true)); }
fn main(){ use(fn(x)->_ {return x;}); }
''')
t.valid('generic_with_universal', '42\ntrue\n42\ntrue\n', text='''
fn apply(f:forall(a)fn(a)->a, x)->_ { return f(x); }
fn main(){
 let id=fn(x)->_ {return x;};
 println(apply(id,42)); println(apply(id,true));
 let local=fn(f:forall(a)fn(a)->a, x)->_ {return f(x);};
 println(local(id,42)); println(local(id,true));
}
''')
t.valid('universal_expected_lambda', '42\ntrue\n', text='''
fn main(){
 let use:fn(forall(a)fn(a)->a)->void = fn(f){println(f(42));println(f(true));};
 use(fn(x)->_ {return x;});
}
''')
t.valid('generic_expected_lambda', '42\ntrue\n42\ntrue\n', text='''
fn main(){
 let use:fn(forall(a)fn(a)->a, _)->_ = fn(f,x)->_ {f(true);return f(x);};
 let id=fn(x)->_ {return x;};println(use(id,42));println(use(id,true));
 let (other,flag):(fn(forall(a)fn(a)->a, _)->_,bool)=(fn(f,x)->_ {f(1);return f(x);},true);
 println(other(id,42));println(other(id,flag));
}
''')
t.valid('forall_multiple_variables', '42\ntrue\n', text='''
fn use(first: forall(a, b) fn(a, b) -> a) { println(first(42, false)); println(first(true, "text")); }
fn main() { use(fn(x, y) -> _ { return x; }); }
''')
t.valid('forall_pattern_parameter', '42\ntrue\n', text='''
fn use((id, flag): (forall(a) fn(a) -> a, bool)) { println(id(42)); println(id(flag)); }
fn main() { use((fn(x) -> _ { return x; }, true)); }
''')
hooks=['-include', t.root/'tests/fixtures/closure_allocator.h', t.root/'tests/fixtures/closure_allocator.c']
t.valid('forall_escape_ownership', '42\ntrue\n', extra=hooks, text='''
extern fn live_environments() -> i64;
fn make(f: forall(a) fn(a) -> (i64, a)) -> fn() -> i64 {
 return fn() -> i64 { let (n, flag) = f(true); assert(flag); return n; };
}
fn main() {
 {
  let number = 42;
  let keep = fn(x) -> _ { return (number, x); };
  let consume = fn(f: forall(a) fn(a) -> (i64, a)) {
   let (n, x) = f(true); println(n); println(x);
  };
  consume(keep);
  let later = make(keep); assert(later() == 42);
 }
 assert(live_environments() == 0);
}
''')
t.valid('forall_function_instantiation', '42\n', extra=hooks, text='''
extern fn live_environments()->i64;
fn use(f:forall(a)fn(a)->a){
 let n=2; let add=fn(x:i64)->i64 {return n+x;};
 let kept=f(add); println(kept(40));
}
fn main(){use(fn(x)->_ {return x;});assert(live_environments()==0);}
''')
t.valid('forall_loop_cleanup', '', extra=hooks, text='''
extern fn live_environments()->i64;
fn use(f:forall(a)fn(a)->(i64,a)){let(n,b)=f(true);assert(b);}
fn main(){var i=0;while i<100 {let n=i; use(fn(x)->_ {return(n,x);});
 assert(live_environments()==0);i=i+1;}}
''')
exe=t.valid('forall_allocation_failure', '', extra=hooks, text='''
fn use(f:forall(a)fn(a)->(i64,a)){f(1);f(true);}
fn main(){let n=42;use(fn(x)->_ {return(n,x);});}
''')
for count in (0,1):
    p=run([exe],env=dict(os.environ,ON1X_TEST_ENV_FAIL_AFTER=str(count)))
    assert p.returncode==72 and 'environment allocation failure' in p.stderr,(count,p.returncode,p.stderr)
t.valid('pattern_capture_ownership', '42\ntrue\n', extra=hooks, text='''
extern fn live_environments() -> i64;
fn main() {
 { let n = 42;
   let (keep, flag) = (fn(x) -> _ { return (n, x); }, true);
   let (m, b) = keep(flag); println(m); println(b);
   let (k, s) = keep("text"); assert(k == 42);
 } assert(live_environments() == 0);
}
''')
reject = [
 ('forall_monomorphic', 'fn use(f: forall(a) fn(a) -> a) {} fn main() { use(fn(x:i64) -> i64 {return x;}); }', 'expected'),
 ('forall_independent', 'fn use(f: forall(a,b) fn(a,b) -> a) {} fn main() { use(fn(x,y) -> _ {return y;}); }', 'quantified'),
 ('forall_mutable', 'fn use(f: forall(a) fn(a) -> a) {} fn main() { var id=fn(x)->_ {return x;}; use(id); }', 'monomorphic environment'),
 ('forall_effectful', 'fn make() -> fn(i64)->i64 {return fn(x:i64)->i64{return x;};} fn use(f:forall(a)fn(a)->a) {} fn main(){use(make());}', 'nonexpansive'),
 ('forall_unused_invalid', 'fn use(f:forall(a)fn(a)->a) {} fn main(){use(fn(x)->_ {return x+1;});}', 'expected'),
 ('forall_overload', 'fn use(f:forall(a)fn(a)->a) {} fn main(){use(fn(x)->_ {println(x);return x;});}', 'println accepts'),
 ('forall_equality_domain', 'fn use(f:forall(a)fn(a,a)->bool) {} fn main(){use(fn(x,y)->_ {return x==y;});}', 'non-scalar equality'),
 ('forall_extern', 'extern fn raw(f:forall(a)fn(a)->a); fn main(){}', 'concrete monotypes'),
 ('forall_result_extern', 'extern fn raw()->forall(a)fn(a)->a; fn main(){}', 'function-valued extern ABI'),
 ('forall_nested', 'fn use(f:forall(a)fn(forall(b)fn(b)->b)->a) {} fn main(){}', 'nested forall'),
 ('forall_hole', 'fn use(f:forall(a)fn(_)->a) {} fn main(){}', 'fully specified'),
 ('forall_duplicate', 'fn use(f:forall(a,a)fn(a)->a) {} fn main(){}', 'duplicate quantified'),
 ('forall_unbound', 'fn use(f:forall(a)fn(b)->a) {} fn main(){}', 'unknown type'),
 ('forall_json', 'fn use(f:forall(a)fn(json)->a) {} fn main(){}', 'resource parameters'),
 ('destruct_effectful', 'fn make(x)->_ {return (x,true);} fn main(){let (f,b)=make(fn(x)->_ {return x;}); f(1); f(true);}', 'expected i64'),
 ('destruct_mutable_capture', 'fn main(){var n=1; let (f,b)=(fn(x)->_ {return (n,x);},true);}', 'capturing mutable'),
 ('unused_global_invalid', 'fn broken(x)->_ {return missing(x);} fn main(){}', 'unknown function'),
 ('forall_capture_weak', 'fn use(f:forall(a)fn(a)->a){} fn outer(x){use(fn(y)->_ {if true{return x;}return y;});} fn main(){}', 'monomorphic environment'),
 ('forall_predicative', 'fn use(f:forall(a)fn(a)->a, g:forall(b)fn(b)->b){ f(g); } fn main(){}', 'predicative'),
 ('destruct_wrong_shape', 'fn main(){let ((a,b),f)=(1,fn(x)->_ {return x;});}', 'expected'),
 ('polymorphic_recursion', 'fn bad(x)->_ {return bad((x,x));} fn main(){}', 'infinite type'),
 ('implicit_rank2', 'fn use(f){f(1);f(true);} fn main(){}', 'expected i64'),
 ('forall_capture_mutable', 'fn use(f:forall(a)fn(a)->(i64,a)){} fn main(){var n=42;use(fn(x)->_ {return(n,x);});}', 'capturing mutable'),
]
for name, text, diagnostic in reject:
    p = run([t.compiler, '--emit-c', t.source(name, text)])
    assert p.returncode == 1 and diagnostic in p.stderr and not p.stdout, (name, p.returncode, p.stderr)
deep=''.join(f'fn f{i}()->_ {{return f{i+1}();}}' for i in range(140))+'fn f140()->i64{return 0;} fn main(){f0();}'
p=run([t.compiler,'--check',t.source('global_dependency_limit',deep)])
assert p.returncode==1 and 'dependency nesting limit' in p.stderr,(p.returncode,p.stderr)
source=t.source('stable_forall', 'fn use(f:forall(a)fn(a)->a){println(f(42));assert(f(true));} fn main(){use(fn(x)->_ {return x;});}')
first=run([t.compiler,'--emit-c',source]);second=run([t.compiler,'--emit-c',source])
assert first.returncode==second.returncode==0 and first.stdout==second.stdout
assert '@' not in first.stdout
types=run([t.compiler,'--types',t.root/'examples/global_polymorphism.on1x'])
assert types.returncode==0 and '@let' not in types.stdout and 'identity : forall(a0) fn(a0) -> a0' in types.stdout
print('Global/destructuring schemes and explicit forall parameters: PASS')
