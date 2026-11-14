from support import NativeTests, run
t = NativeTests()
t.valid('polymorphism', '42\ntrue\ntext\n42\ntrue\n')
t.valid('extern_patterns', '42\n', extra=[t.root / 'tests/fixtures/extern_patterns.c'], text='''
extern fn flattened((x, (flag, text)): (i64, (bool, string)), extra: i64) -> i64;
fn main() { println(flattened((40, (true, "ok")), 2)); }
''')
t.valid('extern_evaluation', '0\n1\n2\n3\n4\n42\n', extra=[t.root / 'tests/fixtures/extern_patterns.c'], text='''
extern fn flattened((x, (flag, text)): (i64, (bool, string)), extra: i64) -> i64;
fn number() -> i64 { println(1); return 40; }
fn flag() -> bool { println(2); return true; }
fn text() -> string { println(3); return "ok"; }
fn extra() -> i64 { println(4); return 2; }
fn argument() -> (i64, (bool, string)) { println(0); return (number(), (flag(), text())); }
fn main() { println(flattened(argument(), extra())); }
''')
t.valid('generic_capture', '42\ntrue\n', text='''
fn main() {
 let n = 42; let keep = fn(x) -> _ { return (n, x); };
 let (a, b) = keep(true); println(a); println(b);
 let pair = keep("text");
}
''')
t.valid('nested_schemes', '42\ntrue\n', text='''
fn main() {
 let outer = fn(x) -> _ { let id = fn(y) -> _ { return y; }; return id(x); };
 println(outer(42)); println(outer(true));
}
''')
t.valid('generic_product', '42\ntrue\n', text='''
fn main() {
 let pair = (fn(x) -> _ { return x; }, true);
 let (a, flag) = pair; println(a(42));
 let (b, flag2) = pair; println(b(true));
}
''')
hooks=['-include',t.root/'tests/fixtures/closure_allocator.h',t.root/'tests/fixtures/closure_allocator.c']
t.valid('specialized_closure_lifetimes', '42\ntrue\n', extra=hooks, text='''
extern fn live_environments() -> i64;
fn main() {
 {
  let amount = 2;
  let add = fn(x: i64) -> i64 { return x + amount; };
  let keep = fn(x) -> _ { return (add, x); };
  let (f, n) = keep(40); let (g, flag) = keep(true);
  println(f(n)); println(flag); assert(g(40) == 42);
 } assert(live_environments() == 0);
}
''')
t.valid('mixed_weak_and_generic', '42\n42\ntrue\n', text='''
fn main() {
 let f = fn(x, y) -> _ { println(x); return y; };
 assert(f(42, true)); let s = f(42, "text");
 println(true);
}
''')
t.valid('scheme_annotations', '42\ntrue\n', text='''
fn main() {
 let id: fn(_) -> _ = fn(x) -> _ { return x; };
 let alias = id;
 println(alias(42)); println(alias(true));
}
''')
reject = [
 ('mutable', 'fn main() { var id = fn(x) -> _ { return x; }; id(1); id(true); }', 'expected i64'),
 ('expansive', 'fn make(x) -> _ { return x; } fn main() { let id = make(fn(x) -> _ { return x; }); id(1); id(true); }', 'expected i64'),
 ('unused_invalid', 'fn main() { let bad = fn(x) -> _ { return (x, println(1)); }; }', 'tuple elements cannot be void'),
 ('mutable_capture', 'fn main() { var n = 1; let f = fn(x) -> _ { return (n, x); }; f(true); }', 'capturing mutable'),
 ('weak_equality', 'fn main() { let eq = fn(x, y) -> _ { return x == y; }; eq(1, 2); eq(true, false); }', 'expected i64'),
 ('unused_print', 'fn main() { let f = fn(x) { println(x); }; }', 'cannot infer'),
 ('unused_invalid_name', 'fn main() { let f = fn(x) { missing(); }; }', 'unknown function'),
 ('unused_bad_return', 'fn main() { let f = fn(x) -> _ { if true { return x; } }; }', 'not all paths'),
 ('resource_capture', 'fn main() { let root = json_parse_or_die("{}"); let f = fn(x) -> _ { json_print(root); return x; }; }', 'capturing JSON'),
 ('weak_capture', 'fn use(x) { println(x); let f = fn(y) -> _ { return (x, y); }; f(1); f(true); } fn main() {}', 'cannot infer parameter x'),
 ('void_specialization', 'fn main() { let id = fn(x) -> _ { return x; }; id(println(1)); }', 'parameter type cannot be void'),
 ('raw_duplicate', 'extern fn raw((a, a): (i64, bool)); fn main() {}', 'duplicate extern parameter'),
 ('raw_shape', 'extern fn raw((a, b): i64); fn main() {}', 'must match'),
 ('raw_function', 'extern fn raw((a, b): (fn() -> void, bool)); fn main() {}', 'flattened extern leaves'),
 ('raw_hole', 'extern fn raw((a, b)); fn main() {}', 'concrete types'),
]
for name,text,diagnostic in reject:
 p=run([t.compiler,'--emit-c',t.source('reject_'+name,text)])
 assert p.returncode==1 and diagnostic in p.stderr and not p.stdout,(name,p.returncode,p.stderr)
for name,text,diagnostic in [
 ('instance_limit','fn main() { let id = fn(x) -> _ { return x; }; '+'id(1);'*1100+'}', 'specialization limit'),
 ('scheme_limit','fn main() {'+''.join(f'let f{i} = fn(x) -> _ {{ return x; }};' for i in range(600))+'}', 'let scheme'),
 ('alias_depth','fn main() { let f0 = fn(x) -> _ { return x; }; '+''.join(f'let f{i} = f{i-1};' for i in range(1,80))+'f79(1);}', 'specialization'),
]:
 p=run([t.compiler,'--emit-c',t.source(name,text)])
 assert p.returncode==1 and diagnostic in p.stderr and not p.stdout,(name,p.returncode,p.stderr)
print('Let schemes and flattened externs: PASS')
