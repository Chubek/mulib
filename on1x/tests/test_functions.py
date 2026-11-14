#!/usr/bin/env python3
"""P1 monomorphic first-class functions: semantics, strict C11 and FFI limits."""
from support import NativeTests, run

tests = NativeTests()
tests.valid('higher_order', '41\n42\n14\n10\n18\n12\n21\n')
tests.valid('function_shadowing', '9\n9\n11\n11\n1\n', text='''
fn add(x: i64) -> i64 { return x + 1; }
fn subtract(x: i64) -> i64 { return x - 1; }
fn apply(add: fn(i64) -> i64, x: i64) -> i64 { return add(x); }
fn apply_print(println: fn(i64) -> i64, x: i64) -> i64 { return println(x); }
fn main() {
  println(apply(subtract, 10));
  { let add = subtract; println(add(10)); }
  println(add(10));
  println(apply_print(add, 10));
  { let println = add; assert(println(40) == 41); }
  let f = add;
  { let f = f; println((f)(0)); }
}
''')
tests.valid('function_scalar_types', 'text\ntrue\ndone\ndone\n', text='''
fn log() { println("done"); }
fn run(action: fn() -> void) { return action(); }
fn same(text: string) -> string { return text; }
fn negate(value: bool) -> bool { return !value; }
fn string_factory() -> fn(string) -> string { return same; }
fn action_factory() -> fn() -> void { return log; }
fn main() {
  let echo: fn(string) -> string = string_factory();
  println(echo("text"));
  let test: fn(bool) -> bool = negate;
  println(test(false));
  run(action_factory()); action_factory()();
}
''')
tests.valid('function_recursion', '10\n120\n', text='''
fn increment(x: i64) -> i64 { return x + 1; }
fn repeat(f: fn(i64) -> i64, n: i64) -> i64 {
  if n == 0 { return 0; }
  return f(repeat(f, n - 1));
}
fn factorial(n: i64) -> i64 {
  if n == 0 { return 1; }
  let recurse = factorial;
  return n * recurse(n - 1);
}
fn main() { let recurse = repeat; println(recurse(increment, 10)); println(factorial(5)); }
''')
tests.valid('function_control_flow', '1\n2\n4\nfalse\ntrue\n', text='''
fn increment(x: i64) -> i64 { return x + 1; }
fn double(x: i64) -> i64 { return x * 2; }
fn never() -> bool { assert(false); return false; }
fn main() {
  var f = increment; var i = 0;
  while i < 3 {
    println(f(i));
    if i == 0 { f = double; }
    i = i + 1;
  }
  let skipped = never;
  println(false && skipped()); println(true || skipped());
}
''')
tests.valid('nested_function_types', '4\n', text='''
fn increment(x: i64) -> i64 { return x + 1; }
fn keep(f: fn(i64) -> i64) -> fn(i64) -> i64 { return f; }
fn use(h: fn(fn(i64) -> i64) -> fn(i64) -> i64) -> i64 { return h(increment)(3); }
fn main() {
  let typed: fn(fn(i64) -> i64) -> fn(i64) -> i64 = keep;
  println(use(typed));
}
''')
tests.valid('function_evaluation', '0\n1\n2\n3\n4\n5\n1\n7\n2\n3\n4\n5\n', text='''
fn sum(a: i64, b: i64) -> i64 { println(4); return a + b; }
fn pick() -> fn(i64, i64) -> i64 { println(1); return sum; }
fn factory() -> fn() -> fn(i64, i64) -> i64 { println(0); return pick; }
fn argument(tag: i64) -> i64 { println(tag); return tag; }
fn identity(f: fn(i64, i64) -> i64) -> fn(i64, i64) -> i64 { println(7); return f; }
fn main() {
  println(factory()()(argument(2), argument(3)));
  println(identity(pick())(argument(2), argument(3)));
}
''')
tests.valid('raw_function_wrapper', '42\n', extra=[tests.root / 'examples/external.c'], text='''
extern fn on1x_external_double(x: i64) -> i64;
fn wrapper(x: i64) -> i64 { return on1x_external_double(x); }
fn apply(f: fn(i64) -> i64, x: i64) -> i64 { return f(x); }
fn main() { println(apply(wrapper, 21)); }
''')
tests.valid('function_typedef_names', '42\n', extra=[tests.root / 'tests/fixtures/function_names.c'], text='''
extern fn on1x_function_type_1(x: i64) -> i64;
fn wrapper(x: i64) -> i64 { return on1x_function_type_1(x); }
fn apply(f: fn(i64) -> i64, x: i64) -> i64 { return f(x); }
fn main() { println(apply(wrapper, 41)); }
''')
tests.valid('function_cleanup', '0\n0\n0\n', extra=[tests.root / 'tests/fixtures/json_cleanup.c'], text='''
extern fn live_roots() -> i64;
fn zero() -> i64 { return 0; }
fn choose() -> fn() -> i64 {
  let root = json_parse_or_die("{}");
  return zero;
}
fn scoped() { let root = json_parse_or_die("{}"); }
fn invoke(action: fn() -> void) { action(); }
fn main() {
  let f = choose(); println(live_roots()); println(f());
  invoke(scoped); println(live_roots());
}
''')
tests.source('function_library', '''
fn imported(x: i64) -> i64 { return x * 2; }
fn exported() -> fn(i64) -> i64 { return imported; }
''')
tests.valid('function_import', '42\n', text='''
import "function_library.on1x";
fn main() { let f = exported(); println(f(21)); }
''')

prefix = '''
fn increment(x: i64) -> i64 { return x + 1; }
fn flag(x: bool) -> i64 { if x { return 1; } else { return 0; } }
fn no_argument() -> i64 { return 0; }
'''
# These inputs are syntactically valid. Rejection must come from semantic type,
# name, mutability, ownership or ABI checks, not an unsupported-syntax error.
semantic_cases = [
    ('scalar_call', 'fn main() { let f = 1; f(2); }', 'cannot call value of type i64'),
    ('expression_call', 'fn main() { (1 + 2)(3); }', 'cannot call value of type i64'),
    ('result_call', 'fn main() { increment(1)(2); }', 'cannot call value of type i64'),
    ('wrong_arity', 'fn main() { let f = increment; f(); }', 'wrong number of arguments'),
    ('wrong_argument', 'fn main() { let f = increment; f(true); }', 'expected i64, got bool'),
    ('wrong_annotation', 'fn main() { let f: fn(i64) -> bool = increment; }',
     'expected fn(i64) -> bool, got fn(i64) -> i64'),
    ('wrong_higher_argument', '''
fn apply(f: fn(i64) -> i64) {} fn main() { apply(flag); }
''', 'expected fn(i64) -> i64, got fn(bool) -> i64'),
    ('wrong_nested_result', '''
fn keep(f: fn(i64) -> i64) -> fn(i64) -> i64 { return f; }
fn main() { let f: fn(fn(i64) -> i64) -> fn(i64) -> bool = keep; }
''', 'expected fn(fn(i64) -> i64) -> fn(i64) -> bool'),
    ('immutable_function', 'fn main() { let f = increment; f = increment; }', 'immutable'),
    ('wrong_assignment', 'fn main() { var f = increment; f = no_argument; }',
     'expected fn(i64) -> i64, got fn() -> i64'),
    ('wrong_return', 'fn expose() -> fn(i64) -> bool { return increment; } fn main() {}',
     'expected fn(i64) -> bool'),
    ('function_equality', 'fn main() { increment == increment; }', 'non-scalar equality'),
    ('function_print', 'fn main() { println(increment); }', 'println accepts'),
    ('function_arithmetic', 'fn main() { increment + 1; }', 'expected i64'),
    ('function_condition', 'fn main() { if increment {} }', 'expected bool'),
    ('builtin_value', 'fn main() { let f = println; }', 'intrinsic is not a first-class value'),
    ('constructor_value', 'fn main() { let f = json_parse_or_die; }', 'intrinsic is not a first-class value'),
    ('extern_value', 'extern fn raw(x: i64) -> i64; fn main() { let f = raw; }',
     'raw extern functions cannot be used as values'),
    ('extern_returned', '''
extern fn raw(x: i64) -> i64;
fn expose() -> fn(i64) -> i64 { return raw; } fn main() {}
''', 'raw extern functions cannot be used as values'),
    ('extern_callback', 'extern fn raw(f: fn(i64) -> i64); fn main() {}', 'function-valued extern ABI'),
    ('extern_factory', 'extern fn raw() -> fn() -> void; fn main() {}', 'function-valued extern ABI'),
    ('resource_parameter', 'fn reject(f: fn(json) -> void) {} fn main() {}', 'resource parameters'),
    ('resource_return', 'fn reject(f: fn() -> json_view) {} fn main() {}', 'resource returns'),
    ('nested_resource', 'fn reject(f: fn() -> fn() -> json) {} fn main() {}', 'resource returns'),
    ('annotated_resource', 'fn main() { let f: fn(json) -> void = increment; }', 'resource parameters'),
    ('void_parameter', 'fn reject(f: fn(void) -> void) {} fn main() {}', 'parameter type cannot be void'),
    ('global_intrinsic', 'fn println() {} fn main() {}', 'intrinsic name cannot be redefined'),
    ('shadow_intrinsic_scalar', 'fn main() { let println = 1; println(2); }', 'cannot call value of type i64'),
    ('shadow_function_scalar', 'fn main() { let increment = false; increment(2); }',
     'cannot call value of type bool'),
    ('no_let_polymorphism', 'fn main() { let f = increment; f(1); f(true); }', 'expected i64, got bool'),
]
for name, source, expected in semantic_cases:
    path = tests.source('reject_' + name, prefix + source)
    p = run([tests.compiler, '--emit-c', path])
    assert p.returncode == 1 and expected in p.stderr, (name, p.returncode, p.stderr)
    assert not p.stdout, (name, 'partially emitted rejected program')

syntax_cases = [
    ('missing_arrow', 'fn main() { let f: fn(i64) = increment; }', "expected '->'"),
    ('trailing_call_comma', 'fn main() { increment(1,); }', 'expected identifier'),
    ('parameter_names_in_type', 'fn main() { let f: fn(x: i64) -> i64 = increment; }', 'unknown type: x'),
]
for name, source, expected in syntax_cases:
    path = tests.source('syntax_' + name, prefix + source)
    p = run([tests.compiler, '--check', path])
    assert p.returncode == 1 and expected in p.stderr, (name, p.returncode, p.stderr)

text = prefix + 'fn main() {\n  let f = increment;\n  f(true);\n}\n'
source = tests.source('indirect_argument_location', text)
p = run([tests.compiler, '--check', source])
offset = text.index('true')
line, column = text.count('\n', 0, offset) + 1, offset - text.rfind('\n', 0, offset)
assert p.returncode == 1 and f':{line}:{column}: error: expected i64, got bool' in p.stderr, p.stderr

# Reproducible typedef numbering and emission must not depend on address-based
# type identity, parser arena reuse or the native compiler.
source = tests.root / 'examples/higher_order.on1x'
first = run([tests.compiler, '--emit-c', source])
second = run([tests.compiler, '--emit-c', source])
assert first.returncode == second.returncode == 0 and first.stdout == second.stdout
assert 'typedef struct on1x_function_type_' in first.stdout
assert '(*invoke)(on1x_env *' in first.stdout and 'on1x_env *environment;' in first.stdout
print(f'First-class functions: PASS ({len(semantic_cases)} semantic, {len(syntax_cases)} syntax rejections)')
