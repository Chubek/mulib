#!/usr/bin/env python3
"""Tuple parameter bindings: inference, lexical scope and borrowed argument ownership."""
from support import NativeTests, run

t = NativeTests()
hooks = ['-include', t.root / 'tests/fixtures/closure_allocator.h',
         t.root / 'tests/fixtures/closure_allocator.c']
t.valid('parameter_patterns', '42\n42\n42\n', extra=hooks)
t.valid('nested_parameters', '42\n', extra=hooks, text='''
fn sum((a, (b, c)), extra) -> _ { return a + b + c + extra; }
fn main() { println(sum((10, (20, 2)), 10)); }
''')
t.valid('parameter_ownership', '42\n42\n', extra=hooks, text='''
extern fn live_environments() -> i64;
fn fresh(n: i64) -> fn() -> i64 { return fn() -> i64 { return n; }; }
fn identity((f, flag): (fn() -> i64, bool)) -> fn() -> i64 { assert(flag); return f; }
fn wrap((pair, flag): ((fn() -> i64, i64), bool)) -> fn() -> i64 {
  return fn() -> i64 { let (f, n) = pair; assert(flag); return f() + n; };
}
fn main() {
  {
    let n = 40; let f = fn() -> i64 { return n; };
    let saved = identity((f, true)); println(saved() + 2);
    let closure = wrap(((f, 2), true)); println(closure());
    assert(f() == 40);
  }
  {
    let saved = identity((fresh(42), true)); assert(saved() == 42);
    let closure = wrap(((fresh(40), 2), true)); assert(closure() == 42);
  }
  assert(live_environments() == 0);
}
''')
t.valid('parameter_scopes', '42\n42\n', extra=hooks, text='''
fn increment(x: i64) -> i64 { return x + 1; }
fn apply((println, x): (fn(i64) -> i64, i64)) -> i64 { return println(x); }
fn main() {
  println(apply((increment, 41)));
  let n = 1;
  let action = fn((n, x): (i64, i64)) -> i64 { return n + x; };
  println(action((40, 2))); assert(n == 1);
}
''')
t.valid('parameter_return_cleanup', '', extra=hooks, text='''
extern fn live_environments() -> i64;
fn use((f, n): (fn() -> i64, i64), flag: bool) -> i64 {
  if flag { return f() + n; } else { return n; }
}
fn main() {
  var i = 0;
  while i < 100 {
    let n = i;
    assert(use((fn() -> i64 { return n; }, 1), true) == i + 1);
    assert(use((fn() -> i64 { return n; }, 1), false) == 1);
    assert(live_environments() == 0); i = i + 1;
  }
}
''')
cases = [
    ('scalar', 'fn f((a, b): i64) {} fn main() {}', 'requires a tuple'),
    ('arity', 'fn f((a, b): (i64, bool, string)) {} fn main() {}', 'wrong arity'),
    ('nested', 'fn f((a, (b, c)): (i64, bool)) {} fn main() {}', 'requires a tuple'),
    ('duplicate_leaf', 'fn f((x, x): (i64, i64)) {} fn main() {}', 'duplicate local: x'),
    ('duplicate_parameters', 'fn f((x, y): (i64, i64), x: i64) {} fn main() {}', 'duplicate local: x'),
    ('duplicate_patterns', 'fn f((x, y), (z, x)) {} fn main() { f((1, 2), (3, 4)); }', 'duplicate local: x'),
    ('body_redefinition', 'fn f((a, b): (i64, i64)) { let a = 3; } fn main() {}', 'duplicate local: a'),
    ('immutable', 'fn f((a, b): (i64, i64)) { a = b; } fn main() {}', 'immutable binding: a'),
    ('extern', 'extern fn raw((a, b): (fn() -> void, bool)); fn main() {}', 'flattened extern leaves'),
    ('extern_inferred', 'extern fn raw((a, b)); fn main() {}', 'concrete types'),
    ('lambda_duplicate', 'fn main() { let f = fn((x, x): (i64, i64)) {}; }', 'duplicate local: x'),
    ('monomorphic', 'fn main() { var f=fn((x,y)) -> _ {return x;}; f((1,2)); f((true,2)); }', 'expected i64, got bool'),
    ('resource', 'fn f((a, b): (json, i64)) {} fn main() {}', 'tuple elements cannot contain JSON'),
]
for name, text, diagnostic in cases:
    p = run([t.compiler, '--emit-c', t.source('reject_' + name, text)])
    assert p.returncode == 1 and diagnostic in p.stderr and not p.stdout, (name, p.returncode, p.stderr)
deep = 'fn f(' + '(' * 20000 + 'x' + ',y)' * 20000 + ') {} fn main() {}'
p = run([t.compiler, '--check', t.source('deep_parameter_pattern', deep)])
assert p.returncode == 1 and '256-level' in p.stderr, (p.returncode, p.stderr)
source = t.root / 'examples/parameter_patterns.on1x'
first, second = (run([t.compiler, '--emit-c', source]) for _ in range(2))
assert first.returncode == second.returncode == 0 and first.stdout == second.stdout
assert '@argument_' not in first.stdout
print(f'Parameter patterns: PASS ({len(cases)} semantic rejections, borrowed arguments and escaping fields)')
