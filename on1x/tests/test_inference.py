#!/usr/bin/env python3
"""Monomorphic unification, holes, deferred safety checks and strict native C11."""
from support import NativeTests, run

tests = NativeTests()
tests.valid('inference', '42\ntrue\n42\n')
tests.valid('inferred_recursion', '120\ntrue\nfalse\n', text='''
fn factorial(n) -> _ {
  if n == 0 { return 1; }
  return n * factorial(n - 1);
}
fn odd(n) -> _ { if n == 0 { return false; } return even(n - 1); }
fn even(n) -> _ { if n == 0 { return true; } return odd(n - 1); }
fn main() { println(factorial(5)); println(even(10)); println(odd(10)); }
''')
tests.valid('inferred_holes', '42\ntrue\n', text='''
fn identity(x: _) -> _ { return x; }
fn keep(f: fn(_) -> _) -> fn(_) -> _ { return f; }
fn higher(h: fn(fn(_) -> _) -> fn(_) -> _, f) -> _ { return h(f); }
fn inverse(value) -> _ { return !value; }
fn main() {
  let number: _ = identity(42);
  let factory = higher;
  let action: fn(_) -> _ = factory(keep, inverse);
  println(number); println(action(false));
}
''')
tests.valid('inferred_void', 'done\ndone\n', text='''
fn show(x) { println(x); }
fn nothing() -> _ { println("done"); }
fn run(action) -> _ { return action(); }
fn main() -> _ { show("done"); run(nothing); }
''')
tests.valid('inferred_constraints', 'true\ntext\n1\nfalse\n', text='''
fn same(a, b) -> _ { return a == b; }
fn choose(flag, a, b) -> _ { if flag { return a; } else { return b; } }
fn increment(x) -> _ { return x + 1; }
fn decrement(x) -> _ { return x - 1; }
fn negate(x) -> _ { return !x; }
fn main() {
  println(same("a", "a")); println(choose(true, "text", "other"));
  var operation = increment; operation = decrement; println(operation(2));
  { let same = negate; println(same(true)); }
}
''')
tests.valid('inferred_shadowing', '42\n', text='''
fn increment(x) -> _ { return x + 1; }
fn apply(println, x) -> _ { return println(x); }
fn main() {
  println(apply(increment, 41));
  { let println = increment; assert(println(41) == 42); }
}
''')
tests.valid('inferred_forward_use', '42\n', text='''
fn main() { println(identity(42)); }
fn identity(value) -> _ { return value; }
''')
tests.valid('contextual_hole_syntax', '42\n', text='''
fn _(x) -> _ { return x + 1; }
fn main() { let _: _ = _(41); println(_); }
''')
tests.valid('inferred_raw_wrapper', '42\n', extra=[tests.root / 'examples/external.c'], text='''
extern fn on1x_external_double(x: i64) -> i64;
fn wrapper(x) -> _ { return on1x_external_double(x); }
fn apply(operation, x) -> _ { return operation(x); }
fn main() { println(apply(wrapper, 21)); }
''')
tests.valid('inferred_cleanup', '0\n0\n', extra=[tests.root / 'tests/fixtures/json_cleanup.c'], text='''
extern fn live_roots() -> i64;
fn zero() -> i64 { return 0; }
fn choose(flag) -> _ {
  let root: _ = json_parse_or_die("{}");
  if flag { return zero; } else { return zero; }
}
fn main() { let f = choose(true); println(live_roots()); println(f()); }
''')
tests.source('inferred_library', 'fn imported(x) -> _ { return x * 2; }')
tests.valid('inferred_import', '42\n', text='''
import "inferred_library.on1x";
fn alias(f) -> _ { return f; }
fn main() { let operation = alias(imported); println(operation(21)); }
''')

semantic_cases = [
    ('unconstrained_parameter', 'fn unused(x) {} fn main() {}', 'cannot infer parameter x'),
    ('no_generalization', 'fn identity(x) -> _ { return x; } fn main() {}', 'cannot infer parameter x'),
    ('ambiguous_print', 'fn show(x) { println(x); } fn main() {}', 'cannot infer parameter x'),
    ('ambiguous_equality', 'fn same(x, y) -> _ { return x == y; } fn main() {}', 'cannot infer parameter x'),
    ('conflicting_calls', '''
fn identity(x) -> _ { return x; } fn main() { identity(1); identity(true); }
''', 'expected i64, got bool'),
    ('monomorphic_aliases', '''
fn identity(x) -> _ { return x; }
fn main() { let a = identity; let b = identity; a(1); b(false); }
''', 'expected i64, got bool'),
    ('mutable_monomorphism', '''
fn identity(x) -> _ { return x; }
fn boolean(x: bool) -> bool { return x; }
fn main() { var f = identity; f(1); f = boolean; }
''', 'expected i64, got bool'),
    ('occurs_check', 'fn omega(f) -> _ { return f(f); } fn main() {}', 'infinite type'),
    ('recursive_return', 'fn recurse() -> _ { return recurse; } fn main() {}', 'infinite type'),
    ('indirect_cycle', '''
fn first() -> _ { return second; } fn second() -> _ { return first; } fn main() {}
''', 'infinite type'),
    ('unconstrained_recursive_result', 'fn recurse() -> _ { return recurse(); } fn main() {}', 'cannot infer return type'),
    ('implicit_void_preserved', 'fn increment(x) { return x + 1; } fn main() { increment(1); }', 'expected void'),
    ('inferred_missing_return', 'fn bad(x) -> _ { if x { return 1; } } fn main() { bad(true); }', 'not all paths return'),
    ('extern_parameter', 'extern fn raw(x); fn main() { raw(1); }', 'extern signatures require concrete types'),
    ('extern_result', 'extern fn raw() -> _; fn main() { raw(); }', 'extern signatures require concrete types'),
    ('extern_nested_hole', 'extern fn raw(f: fn(_) -> i64); fn main() {}', 'extern signatures require concrete types'),
    ('raw_function_value', '''
extern fn raw(x: i64) -> i64;
fn pass(x) -> _ { return x; } fn main() { pass(raw); }
''', 'raw extern functions cannot be used as values'),
    ('intrinsic_value', 'fn pass(x) -> _ { return x; } fn main() { pass(println); }', 'intrinsic is not a first-class value'),
    ('inferred_resource_parameter', '''
fn consume(x) { json_print(x); } fn main() { let root = json_parse_or_die("{}"); consume(root); }
''', 'unsupported parameter type'),
    ('inferred_resource_return', 'fn bad() -> _ { return json_parse_or_die("{}"); } fn main() {}', 'resource returns'),
    ('nested_resource_signature', '''
fn use(f) { f(json_parse_or_die("{}")) + 0; } fn main() {}
''', 'resource parameters'),
    ('inferred_view_return', '''
fn bad() -> _ { let root = json_parse_or_die("{}"); return json_get_or_die(root, "key"); }
fn main() {}
''', 'resource returns'),
    ('resource_alias', '''
fn main() { let root: _ = json_parse_or_die("{}"); let alias = root; }
''', 'resource aliases'),
    ('mutable_resource', 'fn main() { var root: _ = json_parse_or_die("{}"); }', 'mutable resources'),
    ('discarded_resource', '''
fn bad(text) { json_parse_or_die(text); } fn main() { bad("{}"); }
''', 'discarding an owned'),
    ('resource_temporary', '''
fn main() { let view: _ = json_get_or_die(json_parse_or_die("{}"), "key"); }
''', 'named live owner'),
    ('late_intrinsic_constraint', 'fn show(x) { println(x); } fn zero() {} fn main() { show(zero); }', 'println accepts'),
    ('late_equality_constraint', '''
fn same(x, y) -> _ { return x == y; } fn zero() {} fn main() { same(zero, zero); }
''', 'non-scalar equality'),
    ('late_void_binding', 'fn nothing() -> _ {} fn main() { let x = nothing(); }', 'cannot bind a void'),
    ('late_void_parameter', '''
fn nothing() -> _ {} fn use(x) {} fn main() { use(nothing()); }
''', 'unsupported parameter type'),
    ('late_immutable_assignment', '''
fn inc(x) -> _ { return x + 1; } fn main() { let f = inc; f = inc; }
''', 'immutable'),
    ('non_callable', 'fn main() { let x: _ = 1; x(); }', 'cannot call value of type i64'),
    ('wrong_arity', 'fn apply(f) { f(1); f(1, 2); } fn main() {}', 'wrong number of arguments'),
]
for name, text, diagnostic in semantic_cases:
    path = tests.source('reject_' + name, text)
    p = run([tests.compiler, '--emit-c', path])
    assert p.returncode == 1 and diagnostic in p.stderr, (name, p.returncode, p.stderr)
    assert not p.stdout, (name, 'partial code emitted on inference failure')

syntax_cases = [
    ('missing_parameter', 'fn bad(: _) {} fn main() {}', 'expected identifier'),
    ('missing_function_result', 'fn main() { let f: fn(_) -> = 1; }', 'unknown type: ='),
]
for name, text, diagnostic in syntax_cases:
    path = tests.source('syntax_' + name, text)
    p = run([tests.compiler, '--check', path])
    assert p.returncode == 1 and diagnostic in p.stderr, (name, p.returncode, p.stderr)

# Inspect concrete types, not just a successful native result. Extern evidence
# remains labelled explicitly; no inference variables appear in this output.
path = tests.source('types', '''
extern fn raw(x: i64) -> i64;
fn apply(f, x) -> _ { return f(x); }
fn increment(x) -> _ { return x + 1; }
fn unreferenced(x) -> _ { return x * 2; }
fn main() { println(apply(increment, 41)); }
''')
expected = ('extern raw : fn(i64) -> i64\n'
            'apply : fn(fn(i64) -> i64, i64) -> i64\n'
            'increment : fn(i64) -> i64\nunreferenced : fn(i64) -> i64\nmain : fn() -> void\n')
first = run([tests.compiler, '--types', path])
second = run([tests.compiler, '--types', path])
assert first.returncode == second.returncode == 0 and first.stdout == second.stdout == expected, first.stderr
output = tests.work / 'types.txt'
p = run([tests.compiler, '--types', path, '-o', output])
assert p.returncode == 0 and not p.stdout and output.read_text() == expected, p.stderr

# A long function identifier must not be copied into every parameter's
# diagnostic metadata (quadratic amplification), or into this simple error.
path = tests.source('long_function_name', 'fn ' + 'x' * 8192 + '(' +
                    ','.join(f'parameter_{i}' for i in range(256)) + ') {} fn main() {}')
p = run([tests.compiler, '--check', path])
assert p.returncode == 1 and 'cannot infer parameter parameter_0' in p.stderr, p.stderr
assert len(p.stderr) < 512, len(p.stderr)

# Grounding must diagnose inferred graph growth before recursive HIR/backend
# traversal or diagnostic rendering can expand it without bound.
chain = ['fn base() -> _ { return 0; }']
for i in range(300):
    previous = 'base' if i == 0 else f'chain_{i - 1}'
    chain.append(f'fn chain_{i}() -> _ {{ return {previous}; }}')
path = tests.source('deep_inferred_type', '\n'.join(chain) + '\nfn main() {}')
p = run([tests.compiler, '--check', path])
assert p.returncode == 1 and '257-level limit' in p.stderr, (p.returncode, p.stderr)

growth = ['fn base() -> i64 { return 0; }']
for i in range(12):
    previous = 'base' if i == 0 else f'growth_{i - 1}'
    growth.append(f'fn growth_{i}(f) -> _ {{ f({previous}, {previous}) + 0; return {previous}; }}')
path = tests.source('large_inferred_type', '\n'.join(growth) + '\nfn main() {}')
p = run([tests.compiler, '--check', path])
assert p.returncode == 1 and 'expanded-size limit' in p.stderr, (p.returncode, p.stderr)

# A shared, finite graph below the limits still grounds, reuses structural
# typedefs and compiles without exponentially expanding C declarations.
bounded = growth[:7]
aliases = '\n'.join(f'let alias_{i}: _ = growth_5;' for i in range(32))
tests.valid('inferred_type_dag', '', text='\n'.join(bounded) + '\nfn main() {' + aliases + '}')
print(f'Monomorphic inference: PASS ({len(semantic_cases)} semantic, {len(syntax_cases)} syntax rejections; bounded graph growth)')
