#!/usr/bin/env python3
"""Real canonical MuLIB JSON tests, reported as SKIP when not installed."""
import sys
from support import NativeTests, run

tests = NativeTests()
include = tests.root.parent / 'include'
if not (include / 'mulib-json.h').is_file():
    print('MuLIB JSON integration: SKIP (canonical ../include/mulib-json.h absent)')
    sys.exit(77)
extra = ['-I', include, tests.root / 'runtime/on1x_json.c',
         tests.root / 'runtime/on1x_mulib_impl.c', '-lm']
tests.valid('json', '3\n{"items":[1,2,3],"ok":true}\n', extra=extra)
tests.valid('json_inference', '3\n', extra=extra, text='''
fn inspect(text) -> _ {
  let root: _ = json_parse_or_die(text);
  let items: _ = json_get_or_die(root, "items");
  return json_size(items);
}
fn main() { let reader = inspect; println(reader("{\\"items\\":[1,2,3]}")); }
''')
tests.valid('json_closure_locals', '3\n', extra=extra, text='''
fn reader(text) -> _ {
  return fn() -> _ {
    let root = json_parse_or_die(text);
    let items = json_get_or_die(root, "items");
    return json_size(items);
  };
}
fn main() { let read = reader("{\\"items\\":[1,2,3]}"); println(read()); }
''')
tests.valid('json_lifetimes', '2\n1\n3\n3\n3\n', extra=extra, text='''
fn scoped() { let unused = json_parse_or_die("{}"); }
fn early(stop: bool) -> i64 {
  let root = json_parse_or_die("{\\"outer\\":{\\"items\\":[1,2]}}");
  let child = json_get_or_die(root, "outer");
  let items = json_get_or_die(child, "items");
  if stop { let nested = json_parse_or_die("{}"); return json_size(items); }
  return json_size(root);
}
fn main() {
  scoped(); println(early(true)); println(early(false));
  var i = 0;
  while i < 3 {
    let root = json_parse_or_die("[1,2,3]");
    println(json_size(root)); i = i + 1;
  }
}
''')
tests.valid('json_i64_bounds', '-9223372036854775808\n9223372036854774784\n-12\n', extra=extra, text='''
fn main() {
  let lower = json_parse_or_die("-9223372036854775808");
  let upper = json_parse_or_die("9223372036854774784");
  let negative = json_parse_or_die("-12");
  println(json_number(lower)); println(json_number(upper)); println(json_number(negative));
}
''')
cases = [
    ('parse', 'let root = json_parse_or_die("{");', 'parse failure'),
    ('missing', 'let root = json_parse_or_die("{}"); let view = json_get_or_die(root, "missing");', 'missing member'),
    ('fraction', 'let root = json_parse_or_die("1.5"); println(json_number(root));', 'number outside'),
    ('upper', 'let root = json_parse_or_die("9223372036854775808"); println(json_number(root));', 'number outside'),
    ('null', 'let root = json_parse_or_die("null"); println(json_number(root));', 'not a number'),
]
for name, body, diagnostic in cases:
    exe = tests.build('json_error_' + name, text='fn main() { ' + body + ' }', extra=extra)
    p = run([exe])
    assert p.returncode == 71 and diagnostic in p.stderr, (name, p.returncode, p.stderr)
    # or_die terminates the process; general cleanup on this path is not yet
    # promised. LSan's exit-time report must not be mistaken for a passed test.
    assert 'Sanitizer' not in p.stderr, (name, p.stderr)

# Inject failure into only the canonical kernel's allocators. This replaces,
# rather than accompanies, the normal implementation TU to avoid duplicate
# implementation switches/symbols. Fatal or_die paths do not promise unwinding.
allocation_extra = ['-I', include, tests.root / 'runtime/on1x_json.c',
                    tests.root / 'tests/fixtures/json_alloc.c', '-lm']
for name, body, diagnostic in [
    ('parse', 'fail_json_allocations(); let root = json_parse_or_die("{}");', 'parse failure'),
    ('print', 'let root = json_parse_or_die("{}"); fail_json_allocations(); json_print(root);',
     'stringification failed'),
]:
    exe = tests.build('json_allocation_' + name, extra=allocation_extra, text=
                      'extern fn fail_json_allocations(); fn main() { ' + body + ' }')
    p = run([exe])
    assert p.returncode == 71 and diagnostic in p.stderr, (name, p.returncode, p.stderr)
    assert 'Sanitizer' not in p.stderr, (name, p.stderr)
print('MuLIB JSON integration: PASS')
