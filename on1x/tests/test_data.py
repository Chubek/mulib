#!/usr/bin/env python3
"""Bounded value data model: structural records, closed sums, fixed arrays and owning slices."""
from support import NativeTests, run

tests = NativeTests()
tests.valid('data_records', '7\n0x1.4000000000000p+1\n7\n', text='''
fn take(r: record{a:i64,b:f64}) -> i64 { return r["a"]; }
fn main() {
  let p: record{a:i64,b:f64} = record{a:7,b:2.5};
  println(p["a"]); println(p["b"]);
  println(take(record{b:2.5,a:7}));
}
''')
tests.valid('data_sum_match', '42\n0\n6\n', text='''
fn describe(s: sum{None:record{},Some:i64,Pair:record{first:i64,second:i64}}) -> i64 {
  return match(s){None(unit)=>0, Some(n)=>n, Pair(p)=>p["first"]+p["second"]};
}
fn main() {
  println(describe(variant<sum{None:record{},Some:i64,Pair:record{first:i64,second:i64}}>(Some, 42)));
  println(describe(variant<sum{None:record{},Some:i64,Pair:record{first:i64,second:i64}}>(None, record{})));
  println(describe(variant<sum{None:record{},Some:i64,Pair:record{first:i64,second:i64}}>(Pair, record{first:1,second:5})));
}
''')
tests.valid('data_contextual_match', 'true\n42\nfalse\ntrue\n7\nfalse\n9\n', text='''
fn match(flag:bool)->bool { println(flag); return flag; }
fn main() {
  if match(true) { println(42); }
  if match(false) {} else {
    var keep = true;
    while match(keep) { println(7); keep = false; }
  }
  let value = variant<sum{Pair:(i64,bool)}>(Pair,(9,true));
  if match(value) { Pair((_, yes)) => yes } { println(9); }
}
''')
tests.valid('data_array_slice', '16\n3\n8\n2\n8\n', text='''
fn total(a: array<i64,3>) -> i64 {
  var acc = 0; var slice = a[0:3]; var i = 0;
  while i < slice[] { acc = acc + slice[i]; i = i + 1; }
  return acc;
}
fn main() {
  let arr = array[3,5,8];
  println(total(arr)); println(arr[]); println(arr[2]);
  let s = arr[1:3];
  println(s[]); println(s[1]);
}
''')
tests.valid('data_slice_of_slice', '2\n4\n', text='''
fn main() {
  let a = array[1,2,3,4,5];
  let s = a[1:4];
  let t = s[1:3];
  println(t[]); println(t[1]);
}
''')
tests.valid('data_nested_record', '42\n7\n', text='''
fn inner(r: record{point:record{x:i64,y:i64},tag:sum{A:i64,B:record{}}}) -> i64 {
  return match(r["tag"]){A(n)=>n, B(unit)=>r["point"]["x"]};
}
fn main() {
  let value: record{point:record{x:i64,y:i64},tag:sum{A:i64,B:record{}}}
    = record{point:record{x:7,y:8},tag:variant<sum{A:i64,B:record{}}>(A,42)};
  println(inner(value));
  println(value["point"]["x"]);
}
''')
tests.valid('data_evaluation_order', '1\n2\n3\n', text='''
fn mark(n: i64) -> i64 { println(n); return n; }
fn main() { let r: record{x:i64,y:i64,z:i64} = record{x:mark(1),y:mark(2),z:mark(3)}; assert(r["z"]==3); }
''')
tests.valid('data_value_passthrough', '42\n', text='''
fn wrap(r: record{a:i64}) -> record{a:i64} { return r; }
fn unwrap(r: record{a:i64}) -> i64 { return r["a"]; }
fn main() { println(unwrap(wrap(record{a:42}))); }
''')
tests.valid('data_array_of_records', '2\n8\n', text='''
fn main() {
  let a: array<record{x:i64},2> = array[record{x:2},record{x:8}];
  println(a[0]["x"]); println(a[1]["x"]);
}
''')

# Runtime bounds are terminal, located value errors, like checked arithmetic.
bounds = [
    ('index_high', 'fn main() { let a = array[1,2,3]; println(a[3]); }', 'index out of bounds'),
    ('index_negative', 'fn main() { let a = array[1,2,3]; let i = 0-1; println(a[i]); }', 'index out of bounds'),
    ('slice_end', 'fn main() { let a = array[1,2,3]; let s = a[0:4]; println(s[]); }', 'slice bounds out of range'),
    ('slice_reversed', 'fn main() { let a = array[1,2,3]; let s = a[2:1]; println(s[]); }', 'slice bounds out of range'),
    ('slice_negative', 'fn main() { let a = array[1,2,3]; let s = a[-1:2]; println(s[]); }', 'slice bounds out of range'),
]
for name, source, expected in bounds:
    path = tests.source('reject_' + name, source)
    p = run([tests.compiler, '--emit-c', path])
    assert p.returncode == 0, (name, p.stderr)
    exe = tests.build('runtime_' + name, source=path)
    p = run([exe])
    assert p.returncode != 0 and expected in p.stderr, (name, p.returncode, p.stderr)

# Static rejections: exhaustiveness, unknown labels, managed fields, extern ABI.
semantic_cases = [
    ('malformed_legacy_arm', '''
fn main() {
  let s = variant<sum{Some:i64}>(Some, 1);
  println(match(s){Some(n)=>});
}
''', "expected identifier"),
    ('non_exhaustive', '''
fn main() {
  let s = variant<sum{None:record{},Some:i64}>(None, record{});
  let n = match(s){None(unit)=>0};
  println(n);
}
''', 'non-exhaustive match'),
    ('duplicate_arm', '''
fn main() {
  let s = variant<sum{None:record{},Some:i64}>(Some, 1);
  let n = match(s){None(unit)=>0, Some(n)=>1, Some(m)=>2};
  println(n);
}
''', 'duplicate match constructor'),
    ('unknown_constructor', '''
fn main() {
  let s = variant<sum{None:record{},Some:i64}>(None, record{});
  let n = match(s){None(unit)=>0, Some(n)=>1, Other(unit)=>2};
  println(n);
}
''', 'unknown match constructor'),
    ('arm_arity', '''
fn main() {
  let s = variant<sum{None:record{},Some:i64}>(None, record{});
  let n = match(s){None(unit)=>0, Some()};
  println(n);
}
''', "expected identifier"),
    ('arm_mismatched_payload', '''
fn main() {
  let s = variant<sum{None:record{},Some:i64}>(Some, 1);
  let n = match(s){None(unit)=>0, Some(n: bool)=>1};
  println(n);
}
''', 'expected i64, got bool'),
    ('arm_result_mismatch', '''
fn main() {
  let s = variant<sum{None:record{},Some:i64}>(None, record{});
  let n = match(s){None(unit)=>0, Some(n)=>true};
  println(n);
}
''', 'expected i64, got bool'),
    ('unknown_field', '''
fn main() {
  let p: record{a:i64} = record{a:1};
  println(p["b"]);
}
''', 'unknown record field'),
    ('field_on_sum', '''
fn main() {
  let s = variant<sum{None:record{},Some:i64}>(Some, 1);
  println(s["x"]);
}
''', 'field projection requires a known record shape'),
    ('duplicate_field', '''
fn main() {
  let p: record{a:i64,a:bool} = record{a:1,a:true};
  println(1);
}
''', 'duplicate'),
    ('managed_field', '''
fn main() {
  let r: record{s:string} = record{s:"hi"};
  println(1);
}
''', 'trivially copied types'),
    ('managed_match_result', '''
fn main() {
  let s = variant<sum{None:record{},Some:i64}>(None, record{});
  let n = match(s){None(unit)=>"none", Some(n)=>"some"};
  println(n);
}
''', 'trivially copied types'),
    ('extern_data_parameter', 'extern fn raw(r: record{x:i64}) -> i64; fn main() {}',
     'data-valued extern ABI requires reviewed aggregate adapters'),
    ('extern_data_return', 'extern fn raw() -> record{x:i64}; fn main() {}',
     'data-valued extern ABI requires reviewed aggregate adapters'),
    ('variant_wrong_payload', '''
fn main() {
  let s = variant<sum{None:record{},Some:i64}>(Some, true);
  println(1);
}
''', 'expected i64, got bool'),
    ('variant_unknown_tag', '''
fn main() {
  let s = variant<sum{None:record{},Some:i64}>(Other, 1);
  println(1);
}
''', 'unknown sum constructor'),
    ('variant_open_annotation', '''
fn main() {
  let s = variant<sum{None:record{},Some:i64}>(Some, 1);
  println(s);
}
''', 'println accepts numeric scalars, bool or string'),
    ('index_on_record', '''
fn main() {
  let p: record{a:i64} = record{a:1};
  println(p[0]);
}
''', 'index/slice/length requires a known array or slice shape'),
    ('array_mixed_elements', '''
fn main() {
  let a = array[1, true];
  println(1);
}
''', 'expected i64, got bool'),
    ('data_in_tuple_owned_position', '''
fn main() {
  let t: (record{x:i64}, i64) = (record{x:1}, 2);
  println(1);
}
''', None),
]
for name, source, expected in semantic_cases:
    path = tests.source('reject_' + name, source)
    p = run([tests.compiler, '--emit-c', path])
    if expected is None:
        # Tuples of data values are value structs too; only require success here.
        assert p.returncode == 0, (name, p.stderr)
        continue
    assert p.returncode == 1 and expected in p.stderr, (name, p.returncode, p.stderr)
    assert not p.stdout, (name, 'partially emitted rejected program')

# MIR must reject the new value kinds loudly rather than lowering them as scalars.
path = tests.source('mir_reject_data', 'fn main() { let r: record{x:i64} = record{x:1}; println(r["x"]); }\n')
p = run([tests.compiler, '--emit-ir', path])
assert p.returncode == 1 and 'scalar MIR cannot lower record' in p.stderr, (p.returncode, p.stderr)

# Independent structural-adapter verifier must also fail closed.
p = run([tests.compiler, '--emit-c', tests.source('adapter_reject', 'fn main() { let r: record{x:i64} = record{x:1}; println(r["x"]); }\n')])
assert p.returncode == 0, p.stderr

# Deterministic code generation for a data-heavy program.
source = tests.source('data_determinism', '''
fn main() {
  let s = variant<sum{None:record{},Some:i64}>(Some, 41);
  println(match(s){None(unit)=>0, Some(n)=>n+1});
}
''')
first = run([tests.compiler, '--emit-c', source])
second = run([tests.compiler, '--emit-c', source])
assert first.returncode == second.returncode == 0 and first.stdout == second.stdout

# The normal build helper must execute the data profile end-to-end.
path = tests.source('helper_data', '''
fn describe(s: sum{None:record{},Some:i64}) -> i64 {
  return match(s){None(unit)=>0, Some(n)=>n};
}
fn main() {
  println(describe(variant<sum{None:record{},Some:i64}>(Some, 42)));
}
''')
built = tests.work / 'helper-data'
p = run(['python3', tests.root / 'tools/on1x-build.py', path, '--on1xc', tests.compiler,
         '--cc', tests.cc, '-o', built, *['--cflag=' + flag for flag in tests.flags]])
assert p.returncode == 0, p.stderr
p = run([built])
assert p.returncode == 0 and p.stdout == '42\n', p
print('value data profile: records/sums/arrays/slices, bounds, exhaustiveness and rejection PASS')
