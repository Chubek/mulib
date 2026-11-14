#!/usr/bin/env python3
"""Closures: capture semantics, strict C11, reference ownership and ABI limits."""
import os
from support import NativeTests, run

tests = NativeTests()
allocator = ['-include', tests.root / 'tests/fixtures/closure_allocator.h',
             tests.root / 'tests/fixtures/closure_allocator.c']
counts = '''
extern fn live_environments() -> i64;
extern fn peak_environments() -> i64;
extern fn environment_allocations() -> i64;
'''


def valid(name, expected='', text=None, extra=()):
    if text is not None:
        text = counts + text
    return tests.valid(name, expected, text=text, extra=[*allocator, *extra])


valid('closures', '42\n42\nhello from a closure\n')
valid('anonymous_functions', '42\ntrue\ntext\ndone\n', text='''
fn main() {
  let increment = fn(x: i64) -> i64 { return x + 1; };
  println(increment(41));
  println((fn(x) -> _ { return !x; })(false));
  println(fn(x: string) -> string { return x; }("text"));
  fn() { println("done"); }();
  assert(environment_allocations() == 0);
}
''')
valid('transitive_capture', '42\n', text='''
fn factory(x) -> _ {
  let y = 2;
  return fn(z) -> _ { return fn(w) -> _ { return x + y + z + w + x; }; };
}
fn main() {
  { let outer = factory(10); let inner = outer(20); println(inner(0)); }
  assert(live_environments() == 0);
}
''')
valid('closure_ownership', '42\n43\n', text='''
fn add(n) -> _ { return fn(x) -> _ { return n + x; }; }
fn keep(f) -> _ { return f; }
fn main() {
  {
    let first = add(1); var action = first; let alias = keep(action);
    action = action; assert(live_environments() == 1);
    action = add(2); assert(live_environments() == 2);
    let old = action; action = first;
    println(alias(41)); println(old(41));
    assert(live_environments() == 2);
  }
  assert(live_environments() == 0);
}
''')
valid('closure_returns', '42\n42\n', text='''
fn add(n) -> _ { return fn(x) -> _ { return n + x; }; }
fn save(f) -> _ { return fn() -> _ { return f; }; }
fn choose(flag, f, g) -> _ { if flag { return f; } else { return g; } }
fn main() {
  {
    let saved = save(add(2)); let copy = saved(); println(copy(40));
    println(choose(false, add(1), add(2))(40));
  }
  assert(live_environments() == 0);
}
''')
valid('closure_scalar_captures', 'text\nother\ndone\n', text='''
fn format(text, flag) -> _ { return fn() -> _ { if flag { return text; } else { return "other"; } }; }
fn announce(text) -> _ { return fn() { println(text); }; }
fn run(action) { return action(); }
fn main() {
  println(format("text", true)()); println(format("unused", false)());
  run(announce("done")); assert(live_environments() == 0);
}
''')
valid('closure_lexical_shadowing', '2\n1\n11\n', text='''
fn increment(x: i64) -> i64 { return x + 1; }
fn main() {
  {
    let x = 1;
    let f = fn() -> i64 {
      let y = x;
      { let x = 2; let g = fn() -> i64 { return x; }; println(g()); }
      return y;
    };
    assert(f() == 1); println(x);
    let println = fn(x: i64) -> i64 { return increment(x); };
    let action = fn() -> i64 { return println(10); };
    assert(action() == 11);
  }
  println(11); assert(live_environments() == 0);
}
''')
valid('closure_snapshots', '1\n42\n', text='''
fn main() {
  {
    var number = 1; let snapshot = number;
    let action = fn() -> i64 { return snapshot; };
    number = 2; println(action()); assert(number == 2);
    var f = fn(x: i64) -> i64 { return x + 1; };
    let saved = f; let use = fn() -> i64 { return saved(41); };
    f = fn(x: i64) -> i64 { return x - 1; };
    println(use()); assert(f(41) == 40);
  }
  assert(live_environments() == 0);
}
''')
valid('closure_local_mutation', '42\n', text='''
fn factory(base) -> _ {
  return fn(n) -> _ {
    var total = base; var i = 0;
    while i < n { total = total + 1; i = i + 1; }
    return total;
  };
}
fn main() { println(factory(40)(2)); assert(live_environments() == 0); }
''')
valid('closure_condition_temporaries', text='''
fn running(n: i64) -> fn() -> bool { return fn() -> bool { return n < 200; }; }
fn main() {
  var i = 0;
  while running(i)() { assert(live_environments() == 0); i = i + 1; }
  assert(environment_allocations() == 201); assert(peak_environments() == 1);
  if running(0)() { assert(live_environments() == 0); }
  else { assert(false); }
}
''')
valid('closure_short_circuit', text='''
fn test(n: i64) -> fn() -> bool { return fn() -> bool { return n == 1; }; }
fn use(action: fn() -> bool, flag: bool) -> bool { return action() && flag; }
fn choose(n: i64) -> fn(bool) -> bool { return fn(flag: bool) -> bool { return flag && n == 1; }; }
fn never() -> bool { assert(false); return false; }
fn main() {
  assert(use(test(1), false || test(1)()));
  assert(choose(1)(false || test(1)()));
  assert(use(test(1), (true && test(1)()) || never()));
  assert(!(false && test(1)())); assert(true || test(1)());
  assert(live_environments() == 0);
}
''')
valid('closure_evaluation_order', '0\n1\n2\n3\n4\n42\n', text='''
fn factory() -> _ {
  println(0); let base = 37;
  return fn() -> _ {
    println(1);
    return fn(a: i64, b: i64) -> i64 { println(4); return base + a + b; };
  };
}
fn argument(tag: i64) -> i64 { println(tag); return tag; }
fn main() { println(factory()()(argument(2), argument(3))); assert(live_environments() == 0); }
''')
valid('closure_discard_temporaries', text='''
fn add(n: i64) -> fn(i64) -> i64 { return fn(x: i64) -> i64 { return n + x; }; }
fn main() {
  var i = 0;
  while i < 1000 { add(i); assert(live_environments() == 0); i = i + 1; }
  assert(environment_allocations() == 1000); assert(peak_environments() == 1);
}
''')
valid('closure_top_level_recursion', '120\n', text='''
fn factorial(n: i64) -> i64 {
  let recurse = fn(value: i64) -> i64 { return factorial(value); };
  if n == 0 { return 1; } return n * recurse(n - 1);
}
fn main() { println(factorial(5)); assert(environment_allocations() == 0); }
''')
valid('closure_raw_calls', '42\n', extra=[tests.root / 'examples/external.c'], text='''
extern fn on1x_external_double(x: i64) -> i64;
fn factory(x) -> _ { return fn() -> _ { return on1x_external_double(x); }; }
fn main() { println(factory(21)()); assert(live_environments() == 0); }
''')
valid('closure_names', '42\n', text='''
extern fn on1x_function_type_1(x: i64) -> i64;
extern fn on1x_fn_main();
extern fn on1x_lambda_1();
extern fn on1x_make_lambda_1();
extern fn on1x_drop_lambda_1();
extern fn on1x_lambda_env_1();
extern fn on1x_adapter_main();
extern fn on1x_v1_tmp();
fn main() { let n = 42; let f = fn() -> i64 { return n; }; println(f()); }
''')
valid('closure_json_cleanup', '0\n42\n0\n', extra=[tests.root / 'tests/fixtures/json_cleanup.c'], text='''
extern fn live_roots() -> i64;
fn factory(n: i64) -> fn() -> i64 {
  let root = json_parse_or_die("{}");
  return fn() -> i64 { let local = json_parse_or_die("{}"); return n; };
}
fn main() { { let f = factory(42); println(live_roots()); println(f()); }
  println(live_roots()); assert(live_environments() == 0); }
''')
tests.source('closure_library', 'fn exported(n) -> _ { return fn(x) -> _ { return n + x; }; }')
valid('closure_import', '42\n', text='''
import "closure_library.on1x";
fn main() { println(exported(2)(40)); assert(live_environments() == 0); }
''')
valid('deep_environment_release', text='''
fn terminal() -> i64 { return 0; }
fn wrap(inner: fn() -> i64) -> fn() -> i64 { return fn() -> i64 { return inner(); }; }
fn main() {
  {
    var action = terminal; var i = 0;
    while i < 50000 { action = wrap(action); i = i + 1; }
    assert(live_environments() == 50000);
  }
  assert(live_environments() == 0);
}
''')

semantic_cases = [
    ('mutable_scalar', 'fn main() { var x = 1; let f = fn() -> i64 { return x; }; }', 'capturing mutable binding'),
    ('mutable_function', '''
fn main() { var f = fn() {}; let g = fn() { f(); }; }
''', 'capturing mutable binding'),
    ('mutating_outer', 'fn main() { var x = 1; let f = fn() { x = 2; }; }', 'capturing mutable binding'),
    ('transitive_mutable', '''
fn main() { var x = 1; let f = fn() -> fn() -> i64 { return fn() -> i64 { return x; }; }; }
''', 'capturing mutable binding'),
    ('json_capture', '''
fn main() { let root = json_parse_or_die("{}"); let f = fn() { json_print(root); }; }
''', 'capturing JSON resources or views'),
    ('json_view_capture', '''
fn main() { let root = json_parse_or_die("{}"); let view = json_get_or_die(root, "key");
  let f = fn() -> _ { return json_size(view); }; }
''', 'capturing JSON resources or views'),
    ('transitive_json', '''
fn main() { let root = json_parse_or_die("{}"); let f = fn() -> fn() -> void {
  return fn() { json_print(root); }; }; }
''', 'capturing JSON resources or views'),
    ('resource_parameter', 'fn main() { let f = fn(root: json) {}; }', 'resource parameters'),
    ('resource_result', 'fn main() { let f = fn() -> _ { return json_parse_or_die("{}"); }; }', 'resource returns'),
    ('void_parameter', 'fn main() { let f = fn(x: void) {}; }', 'parameter type cannot be void'),
    ('ambiguous_parameter', 'fn main() { let f = fn(x) -> _ { return x; }; }', 'cannot infer anonymous-function parameter x'),
    ('monomorphic_lambda', '''
fn main() { let f = fn(x) -> _ { return x; }; f(1); f(true); }
''', 'expected i64, got bool'),
    ('monomorphic_alias', '''
fn main() { let f = fn(x) -> _ { return x; }; let g = f; f(1); g(true); }
''', 'expected i64, got bool'),
    ('occurs_check', 'fn main() { let f = fn(x) -> _ { return x(x); }; }', 'infinite type'),
    ('omitted_return_void', 'fn main() { let f = fn(x: i64) { return x; }; }', 'expected void'),
    ('missing_return', 'fn main() { let f = fn(x: bool) -> i64 { if x { return 1; } }; }', 'not all paths return in anonymous function'),
    ('outer_missing_return', '''
fn bad() -> i64 { let f = fn() -> i64 { return 1; }; } fn main() {}
''', 'not all paths return in function: bad'),
    ('local_recursion', 'fn main() { let f = fn() { f(); }; }', 'unknown function: f'),
    ('duplicate_parameter', 'fn main() { let f = fn(x: i64, x: i64) {}; }', 'duplicate parameter: x'),
    ('duplicate_local', 'fn main() { let f = fn(x: i64) { let x = 2; }; }', 'duplicate local: x'),
    ('unreachable_body', 'fn main() { let f = fn() { return; println(1); }; }', 'unreachable statement'),
    ('scope_escape', 'fn main() { { let x = 1; } let f = fn() -> i64 { return x; }; }', 'undefined variable: x'),
    ('raw_name_escape', '''
extern fn raw(x: i64) -> i64;
fn main() { let f = fn() -> fn(i64) -> i64 { return raw; }; }
''', 'raw extern functions cannot be used as values'),
    ('intrinsic_name_escape', 'fn main() { let f = fn() { let action = println; }; }', 'intrinsic is not a first-class value'),
    ('raw_callback', '''
extern fn raw(action: fn() -> void); fn main() { raw(fn() {}); }
''', 'function-valued extern ABI'),
    ('closure_equality', 'fn main() { let f = fn() {}; f == f; }', 'non-scalar equality'),
    ('closure_print', 'fn main() { println(fn() {}); }', 'println accepts'),
    ('wrong_annotation', 'fn main() { let f: fn() -> i64 = fn() -> bool { return true; }; }', 'expected fn() -> i64'),
    ('wrong_argument', 'fn main() { fn(x: i64) {}(true); }', 'expected i64, got bool'),
]
for name, text, diagnostic in semantic_cases:
    source = tests.source('reject_' + name, text)
    p = run([tests.compiler, '--emit-c', source])
    assert p.returncode == 1 and diagnostic in p.stderr, (name, p.returncode, p.stderr)
    assert not p.stdout, (name, 'partially emitted rejected program')

syntax_cases = [
    ('named_lambda', 'fn main() { let f = fn named() {}; }', "expected '('"),
    ('missing_body', 'fn main() { let f = fn() -> i64; }', "expected '{'"),
    ('missing_semicolon', 'fn main() { let f = fn() {} }', "expected ';'"),
    ('trailing_parameter_comma', 'fn main() { let f = fn(x: i64,) {}; }', 'expected identifier'),
]
for name, text, diagnostic in syntax_cases:
    source = tests.source('syntax_' + name, text)
    p = run([tests.compiler, '--check', source])
    assert p.returncode == 1 and diagnostic in p.stderr, (name, p.returncode, p.stderr)

source = tests.source('capture_location', 'fn main() {\n  var x = 1;\n  let f = fn() -> i64 { return x; };\n}')
p = run([tests.compiler, '--check', source])
assert p.returncode == 1 and ':3:32: error: capturing mutable binding' in p.stderr, p.stderr

source = tests.root / 'examples/closures.on1x'
first = run([tests.compiler, '--emit-c', source])
second = run([tests.compiler, '--emit-c', source])
assert first.returncode == second.returncode == 0 and first.stdout == second.stdout
assert 'on1x_env_alloc(' in first.stdout and 'on1x_env_retain(' in first.stdout
assert 'ON1X_CLOSURE_ABI_VERSION != 1' in first.stdout
incompatible = tests.work / 'incompatible-runtime'
incompatible.mkdir(exist_ok=True)
header = (tests.root / 'runtime/on1x_runtime.h').read_text()
(incompatible / 'on1x_runtime.h').write_text(header.replace('#define ON1X_CLOSURE_ABI_VERSION 1',
                                                         '#define ON1X_CLOSURE_ABI_VERSION 2'))
p = run([tests.cc, '-std=c11', '-fsyntax-only', '-I', incompatible,
         tests.work / 'closures.c', *tests.flags])
assert p.returncode != 0 and 'incompatible On1x closure runtime ABI' in p.stderr, p.stderr
types = run([tests.compiler, '--types', source])
assert types.returncode == 0 and types.stdout == (
    'add_by : fn(i64) -> fn(i64) -> i64\n'
    'compose : fn(fn(i64) -> i64, fn(i64) -> i64) -> fn(i64) -> i64\n'
    'main : fn() -> void\n'), types.stderr

# Fatal paths are distinct from lexical cleanup; they terminate, not unwind.
exe = valid('closure_allocation_failure', text='''
fn main() { let n = 42; let first = fn() -> i64 { return n; };
  let second = fn() -> i64 { return first(); }; assert(second() == 42); }
''')
for count in (0, 1):
    p = run([exe], env=dict(os.environ, ON1X_TEST_ENV_FAIL_AFTER=str(count)))
    assert p.returncode == 72 and 'environment allocation failure' in p.stderr, (count, p.returncode, p.stderr)

# Isolate runtime overflow/underflow guards, allocator initialization and a
# 100,000-environment destruction chain without generated-language recursion.
runtime = tests.work / 'closure_runtime'
p = run([tests.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
         '-I', tests.root / 'runtime', tests.root / 'tests/test_closure_runtime.c',
         tests.root / 'runtime/on1x_runtime.c', *allocator, *tests.flags, '-o', runtime])
assert p.returncode == 0, p.stderr
p = run([runtime])
assert p.returncode == 0 and 'iterative destruction PASS' in p.stdout, (p.returncode, p.stderr)
for mode, diagnostic in [('invalid-size', 'invalid environment size'), ('overflow', 'reference overflow'),
                         ('underflow', 'reference underflow'), ('invalid-retain', 'invalid retain')]:
    p = run([runtime, mode])
    assert p.returncode == 72 and diagnostic in p.stderr, (mode, p.returncode, p.stderr)
print(f'Closures: PASS ({len(semantic_cases)} semantic, {len(syntax_cases)} syntax rejections; allocation and reference-count guards)')
