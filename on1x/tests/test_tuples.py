#!/usr/bin/env python3
"""Product values: strict native C11, monomorphic constraints and closure ownership."""
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


valid('tuples', '42\ntrue\n42\n38\n')
valid('tuple_scalars', '42\ntrue\ntext\n', text='''
fn main() {
  let value: (i64, bool, string) = (42, true, "text");
  let (number, flag, text) = value;
  println(number); println(flag); println(text);
  assert(environment_allocations() == 0);
}
''')
valid('tuple_grouping', '42\n', text='''
fn combine(pair: ((i64, i64))) -> (i64) { let (a, b) = pair; return a + b; }
fn main() { let f: (fn((i64, i64)) -> i64) = combine; println((f)((40, (2)))); }
''')
valid('tuple_nested_patterns', '42\ntext\ntrue\n', text='''
fn make() -> (i64, (string, bool)) { return (42, ("text", true)); }
fn main() { let (n, (text, flag)): (_, (_, _)) = make(); println(n); println(text); println(flag); }
''')
valid('tuple_inferred_lambda', '42\n', text='''
fn apply(operation, input) -> _ { return operation(input); }
fn main() {
  let sum: fn((_, _)) -> _ = fn(pair) -> _ { let (a, b) = pair; return a + b; };
  println(apply(sum, (40, 2)));
}
''')
valid('tuple_evaluation_order', '0\n1\n2\n3\n6\n', text='''
fn mark(n: i64) -> i64 { println(n); return n; }
fn factory() -> _ { println(0); return (mark(1), (mark(2), mark(3))); }
fn main() { let (a, (b, c)) = factory(); println(a + b + c); }
''')
valid('tuple_shadowing', '11\n4\n1\n42\n', text='''
fn main() {
  let x = 1;
  { let (x, y) = (x + 10, 4); println(x); println(y); }
  println(x);
  let (_, y) = (40, 2); println(_ + y);
}
''')
valid('tuple_mutable_binding', '42\n', text='''
fn main() {
  var same = 0; same = same; assert(same == 0);
  var pair = (1, false); pair = pair; pair = (42, true);
  let (number, flag) = pair; assert(flag); println(number);
}
''')
valid('tuple_function_ownership', '42\n43\n', text='''
fn add(n) -> _ { return fn(x) -> _ { return x + n; }; }
fn keep(value) -> _ { return value; }
fn main() {
  {
    let original = (add(1), add(2)); var pair = original;
    pair = pair; assert(live_environments() == 2);
    let copy = keep(pair); let (first, second) = copy;
    pair = (add(3), add(4)); assert(live_environments() == 4);
    pair = original; assert(live_environments() == 2);
    println(first(41)); println(second(41));
  }
  assert(live_environments() == 0);
}
''')
valid('tuple_nested_function_fields', '42\n', text='''
fn add(n) -> _ { return fn(x) -> _ { return n + x; }; }
fn factory() -> _ { let f = add(1); return ((f, f), (f, 40)); }
fn main() {
  {
    let (operations, (c, n)) = factory(); let (a, b) = operations;
    assert(live_environments() == 1); println(a(b(c(n - 1))));
  }
  assert(live_environments() == 0);
}
''')
valid('tuple_escaping_capture', '42\n', text='''
fn add(n) -> _ { return fn(x) -> _ { return n + x; }; }
fn factory() -> _ {
  let pair = (add(2), 40);
  return fn() -> _ { let (f, n) = pair; return f(n); };
}
fn main() { { let f = factory(); println(f()); } assert(live_environments() == 0); }
''')
valid('tuple_transitive_capture', '42\n', text='''
fn factory(n) -> _ {
  let pair = (fn(x: i64) -> i64 { return x + n; }, 40);
  return fn() -> _ { return fn() -> _ { let (f, value) = pair; return f(value); }; };
}
fn main() { println(factory(2)()()); assert(live_environments() == 0); }
''')
valid('tuple_snapshot_capture', '42\n', text='''
fn add(n) -> _ { return fn(x) -> _ { return n + x; }; }
fn main() {
  {
    var pair = (add(1), 41); let snapshot = pair;
    let saved = fn() -> _ { return snapshot; };
    pair = (add(10), 1); let (f, n) = saved(); println(f(n));
  }
  assert(live_environments() == 0);
}
''')
valid('tuple_condition_temporaries', text='''
fn factory(n: i64) -> (fn() -> bool, i64) { return (fn() -> bool { return n < 100; }, n); }
fn test(pair: (fn() -> bool, i64)) -> bool { let (condition, n) = pair; return condition() && n < 100; }
fn main() {
  var i = 0;
  while test(factory(i)) { assert(live_environments() == 0); i = i + 1; }
  assert(environment_allocations() == 101); assert(peak_environments() == 1);
  assert(live_environments() == 0);
}
''')
valid('tuple_short_circuit', text='''
fn factory(n: i64) -> (fn() -> bool, bool) { return (fn() -> bool { return n == 1; }, true); }
fn test(pair: (fn() -> bool, bool), flag: bool) -> bool {
  let (action, value) = pair; return flag && value && action();
}
fn main() {
  assert(test(factory(1), false || test(factory(1), true)));
  assert(true || test(factory(1), false));
  assert(live_environments() == 0);
}
''')
valid('tuple_discard_and_return', text='''
fn factory(n: i64) -> (fn() -> i64, bool) { return (fn() -> i64 { return n; }, true); }
fn choose(flag: bool) -> (fn() -> i64, bool) {
  let original = factory(1);
  if flag { return original; } else { return factory(2); }
}
fn main() {
  choose(true); choose(false); assert(live_environments() == 0);
  var i = 0;
  while i < 100 { factory(i); assert(live_environments() == 0); i = i + 1; }
}
''')
valid('tuple_raw_wrapper', '42\n', extra=[tests.root / 'examples/external.c'], text='''
extern fn on1x_external_double(x: i64) -> i64;
fn double(pair) -> _ { let (a, b) = pair; return (on1x_external_double(a), b); }
fn main() { let (n, flag) = double((21, true)); assert(flag); println(n); }
''')
valid('tuple_names', '42\n', text='''
extern fn on1x_tuple_type_1(); extern fn on1x_tuple_type_2();
extern fn on1x_tuple_retain_1(); extern fn on1x_tuple_release_2();
fn main() {
  let n = 42; let pair = (fn() -> i64 { return n; }, true);
  let (f, flag) = pair; assert(flag); println(f());
}
''')
tests.source('tuple_library', 'fn exported(pair) -> _ { let (a, b) = pair; return (b, a); }')
valid('tuple_import', '42\n', text='''
import "tuple_library.on1x";
fn main() { let (n, flag) = exported((true, 42)); assert(flag); println(n); }
''')
valid('tuple_json_cleanup', '0\n42\n0\n', extra=[tests.root / 'tests/fixtures/json_cleanup.c'], text='''
extern fn live_roots() -> i64;
fn factory(n: i64) -> (fn() -> i64, bool) {
  let root = json_parse_or_die("{}"); return (fn() -> i64 { return n; }, true);
}
fn main() { { let (f, flag) = factory(42); assert(flag); println(live_roots()); println(f()); }
  println(live_roots()); assert(live_environments() == 0); }
''')
valid('tuple_capture_chain', text='''
fn terminal() -> i64 { return 0; }
fn wrap(pair: (fn() -> i64, bool)) -> fn() -> i64 {
  return fn() -> i64 { let (action, flag) = pair; assert(flag); return action(); };
}
fn main() {
  {
    var action = terminal; var i = 0;
    while i < 50000 { action = wrap((action, true)); i = i + 1; }
    assert(live_environments() == 50000);
  }
  assert(live_environments() == 0);
}
''')

# Deep products containing a closure use pointer-based ownership helpers and
# borrowed intermediate pattern projections, not repeated aggregate copies or
# a full access path expanded separately into every leaf.
value, pattern = '(f, 0)', '(operation, last)'
for i in range(64):
    value, pattern = f'({value}, 0)', f'({pattern}, n{i})'
valid('bounded_tuple_ownership', '42\n', text='''
fn main() {
  { let answer = 42; let f = fn() -> i64 { return answer; };
''' + f'let value = {value}; let {pattern} = value; println(operation());' + '''
  } assert(live_environments() == 0);
}
''')
assert (tests.work / 'bounded_tuple_ownership.c').stat().st_size < 150000

growth = ['fn base(n: i64) -> _ { return (fn() -> i64 { return n; }, true); }']
for i in range(14):
    previous = 'base' if i == 0 else f'growth_{i-1}'
    growth.append(f'fn growth_{i}(n: i64) -> _ {{ let p = {previous}(n); return (p, p); }}')
valid('shared_tuple_types', text='\n'.join(growth[:7]) + '''
fn main() { growth_5(42); assert(live_environments() == 0); assert(environment_allocations() == 1); }
''')
assert (tests.work / 'shared_tuple_types.c').stat().st_size < 100000
source = tests.source('expanded_tuple_type', '\n'.join(growth) + '\nfn main() {}')
p = run([tests.compiler, '--emit-c', source])
assert p.returncode == 1 and 'expanded-size limit' in p.stderr and not p.stdout, (p.returncode, p.stderr)

chain = ['fn base() -> _ { return 0; }']
for i in range(300):
    previous = 'base' if i == 0 else f'chain_{i-1}'
    chain.append(f'fn chain_{i}() -> _ {{ return ({previous}(), 0); }}')
source = tests.source('deep_inferred_tuple_type', '\n'.join(chain) + '\nfn main() {}')
p = run([tests.compiler, '--check', source])
assert p.returncode == 1 and any(d in p.stderr for d in ['257-level limit', 'inference work limit', 'dependency nesting limit']), (p.returncode, p.stderr)

semantic_cases = [
    ('wrong_pattern_type', 'fn main() { let (a, b) = 1; }', 'tuple binding pattern requires a tuple'),
    ('wrong_pattern_arity', 'fn main() { let (a, b) = (1, 2, 3); }', 'wrong arity'),
    ('inferred_pattern_arity', 'fn use(pair) { let (a, b) = pair; } fn main() { use((1, 2, 3)); }', 'tuple arity'),
    ('nested_pattern_type', 'fn main() { let (a, (b, c)) = (1, 2); }', 'tuple binding pattern requires a tuple'),
    ('duplicate_pattern', 'fn main() { let (a, (b, a)) = (1, (2, 3)); }', 'duplicate local: a'),
    ('underscore_is_name', 'fn main() { let (_, _) = (1, 2); }', 'duplicate local: _'),
    ('same_scope_redefinition', 'fn main() { let a = 1; let (a, b) = (2, 3); }', 'duplicate local: a'),
    ('nonrecursive_pattern', 'fn main() { let (a, b) = (a, 1); }', 'undefined variable: a'),
    ('mutable_pattern', 'fn main() { var (a, b) = (1, 2); }', 'mutable destructuring is unsupported'),
    ('immutable_pattern_field', 'fn main() { let (a, b) = (1, 2); a = 3; }', 'immutable binding: a'),
    ('wrong_annotation', 'fn main() { let p: (i64, bool) = (true, 1); }', 'expected (i64, bool)'),
    ('wrong_assignment', 'fn main() { var p = (1, true); p = (false, 2); }', 'expected (i64, bool)'),
    ('not_callable', 'fn main() { let p = (1, 2); p(); }', 'cannot call value of type (i64, i64)'),
    ('tuple_equality', 'fn main() { (1, 2) == (1, 2); }', 'non-scalar equality'),
    ('tuple_print', 'fn main() { println((1, 2)); }', 'println accepts'),
    ('tuple_condition', 'fn main() { if (true, false) {} }', 'expected bool'),
    ('void_element', 'fn main() { let p = (println(1), 2); }', 'tuple elements cannot be void'),
    ('resource_element', 'fn main() { let p = (json_parse_or_die("{}"), 1); }', 'tuple elements cannot contain JSON'),
    ('view_element', '''
fn main() { let root = json_parse_or_die("{}"); let p = (json_get_or_die(root, "key"), 1); }
''', 'tuple elements cannot contain JSON'),
    ('annotated_resource', 'fn use(p: (i64, json)) {} fn main() {}', 'tuple elements cannot contain JSON'),
    ('nested_resource', 'fn use(p: (i64, (bool, json_view))) {} fn main() {}', 'tuple elements cannot contain JSON'),
    ('resource_function_field', 'fn use(p: (i64, fn() -> json)) {} fn main() {}', 'resource returns'),
    ('void_annotation', 'fn use(p: (void, i64)) {} fn main() {}', 'tuple elements cannot be void'),
    ('raw_parameter', 'extern fn raw(p: (i64, bool)); fn main() {}', 'tuple-valued extern ABI'),
    ('raw_result', 'extern fn raw() -> (i64, bool); fn main() {}', 'tuple-valued extern ABI'),
    ('raw_hole', 'extern fn raw(p: (_, bool)); fn main() {}', 'extern signatures require concrete types'),
    ('raw_function_field', 'extern fn raw(); fn main() { let p = (raw, 1); }', 'raw extern functions cannot be used as values'),
    ('intrinsic_field', 'fn main() { let p = (println, 1); }', 'intrinsic is not a first-class value'),
    ('mutable_tuple_capture', 'fn main() { var p = (1, 2); let f = fn() -> i64 { let (a, b) = p; return a + b; }; }', 'capturing mutable binding'),
    ('monomorphic_pattern', '''
fn main() { var id=fn(x)->_ {return x;}; let (f, n) = (id, 1); f(n); f(true); }
''', 'expected i64, got bool'),
    ('unconstrained_element', 'fn first(p) -> _ { let (a, b) = p; println(b); return a + 1; } fn main() {}', 'cannot infer parameter p'),
    ('tuple_occurs_check', 'fn bad(p) -> _ { let (f, x) = p; f(p); return x; } fn main() {}', 'infinite type'),
    ('tuple_recursive_result', 'fn bad() -> _ { return (1, bad()); } fn main() {}', 'infinite type'),
    ('tuple_main', 'fn main() -> (i64, bool) { return (0, true); }', 'main must be'),
]
for name, text, diagnostic in semantic_cases:
    source = tests.source('reject_' + name, text)
    p = run([tests.compiler, '--emit-c', source])
    assert p.returncode == 1 and diagnostic in p.stderr, (name, p.returncode, p.stderr)
    assert not p.stdout, (name, 'partially emitted rejected program')

syntax_cases = [
    ('empty_tuple', 'fn main() { let p = (); }', 'expected identifier'),
    ('singleton_tuple', 'fn main() { let p = (1,); }', 'expected identifier'),
    ('trailing_tuple_comma', 'fn main() { let p = (1, 2,); }', 'expected identifier'),
    ('empty_pattern', 'fn main() { let () = (1, 2); }', 'expected identifier'),
    ('singleton_pattern', 'fn main() { let (a) = (1, 2); }', "expected ','"),
    ('trailing_pattern_comma', 'fn main() { let (a, b,) = (1, 2); }', 'expected identifier'),
    ('trailing_type_comma', 'fn main() { let p: (i64, bool,) = (1, true); }', 'unknown type: )'),
    ('annotated_parameter_leaf', 'fn use((a: i64, b): (i64, bool)) {} fn main() {}', "expected ','"),
]
for name, text, diagnostic in syntax_cases:
    source = tests.source('syntax_' + name, text)
    p = run([tests.compiler, '--check', source])
    assert p.returncode == 1 and diagnostic in p.stderr, (name, p.returncode, p.stderr)

source = tests.source('duplicate_location', 'fn main() {\n  let (a, (b, a)) = (1, (2, 3));\n}')
p = run([tests.compiler, '--check', source])
assert p.returncode == 1 and ':2:15: error: duplicate local: a' in p.stderr, p.stderr

source = tests.root / 'examples/tuples.on1x'
first, second = (run([tests.compiler, '--emit-c', source]) for _ in range(2))
assert first.returncode == second.returncode == 0 and first.stdout == second.stdout
assert 'typedef struct on1x_tuple_type_' in first.stdout
assert 'on1x_tuple_retain_' in first.stdout and 'on1x_tuple_release_' in first.stdout
types = run([tests.compiler, '--types', source])
assert types.returncode == 0 and types.stdout == (
    'swap : forall(a0, a1) fn((a0, a1)) -> (a1, a0)\n'
    'operations : fn(i64) -> (fn(i64) -> i64, fn(i64) -> i64)\n'
    'main : fn() -> void\n'), types.stderr
print(f'Tuples: PASS ({len(semantic_cases)} semantic, {len(syntax_cases)} syntax rejections; closure-field ownership)')
