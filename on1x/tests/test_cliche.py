#!/usr/bin/env python3
"""Intrinsic core cliches: coherent static selection and strict C11 execution."""
from support import NativeTests, run


t = NativeTests()

runtime_source = r'''
record Box<T> { value: T }

cliche Show {
 fn show(value: Self) -> string;
 let fallback: Self;
}
cliche Code { let code: i64; }
cliche Keep { fn keep<U>(value: Self, other: U) -> U; }
cliche Describe<Other> where Show<Other> {
 fn describe(value: Self, other: Other) -> string;
}

impl Show for i64 {
 fn show(value: i64) -> string { return format("{}", value); }
 let fallback: i64 = 5;
}
impl Show for bool {
 fn show(value: bool) -> string { return "bool"; }
 let fallback: bool = false;
}
impl<T> Show for Box<T> where Show<T> {
 fn show(value: Box<T>) -> string {
  return Show<T>::show(value["value"]);
 }
 let fallback: Box<T> =
  record<Box<T>> { value: Show<T>::fallback };
}
impl Show for Box<i64> {
 fn show(value: Box<i64>) -> string { return "exact"; }
 let fallback: Box<i64> = record<Box<i64>> { value: 42 };
}
impl<T> Code for Box<T> { let code: i64 = 9; }
impl Keep for i64 {
 fn keep<U>(value: i64, other: U) -> U { return other; }
}
impl Describe<bool> for i64 {
 fn describe(value: i64, other: bool) -> string {
  return Show<bool>::show(other);
 }
}

fn render<T>(value: T) -> string where Show<T> {
 return Show<T>::show(value);
}
fn mark(value: i64) -> i64 { println(value); return value; }

fn main() {
 println(render(7));
 println(render(record<Box<bool>> { value: true }));
 println(Show<Box<i64>>::show(record<Box<i64>> { value: 1 }));
 println(Show<Box<i64>>::fallback["value"]);
 println(Code<Box<bool>>::code);
 println(Keep<i64>::keep<bool>(1, true));
 println(Keep<i64>::keep<i64>(mark(1), mark(2)));
 println(Describe<i64,bool>::describe(0, true));
}
'''

t.valid('cliche_runtime', '7\nbool\nexact\n42\n9\ntrue\n1\n2\n2\nbool\n',
        text=runtime_source)

# The same checked source must publish byte-identical C11. Dispatch is through
# generated static names; no source cliche name or runtime string lookup remains.
source = t.work / 'cliche_runtime.on1x'
first, second = t.work / 'cliche-first.c', t.work / 'cliche-second.c'
p = run([t.compiler, '--emit-c', source, '-o', first])
assert p.returncode == 0 and not p.stdout, p.stderr
p = run([t.compiler, '--emit-c', source, '-o', second])
assert p.returncode == 0 and not p.stdout, p.stderr
generated = first.read_text(encoding='utf-8')
assert generated == second.read_text(encoding='utf-8')
assert 'on1x_fn_cliche_' in generated and 'strcmp' not in generated
assert 'Show' not in generated and 'Keep' not in generated

t.source('cliche_library', r'''
cliche Imported {
 fn add(value: Self, extra: i64) -> i64;
}
impl Imported for i64 {
 fn add(value: i64, extra: i64) -> i64 { return value + extra; }
}
''')
t.valid('cliche_import', '42\n', text=r'''
import cliche_library as Lib;
fn main() { println(Lib.Imported<i64>::add(40, 2)); }
''')
t.valid('cliche_selective_import', '42\n', text=r'''
from cliche_library import Imported as Operation;
fn main() { println(Operation<i64>::add(41, 1)); }
''')
t.valid('cliche_local_nominal_impl', '42\n', text=r'''
import cliche_library as Lib;
record Local { value: i64 }
impl Lib.Imported for Local {
 fn add(value: Local, extra: i64) -> i64 {
  return value["value"] + extra;
 }
}
fn main() {
 println(Lib.Imported<Local>::add(record<Local> { value: 40 }, 2));
}
''')


def reject(name, text, diagnostic):
    path = t.source('reject_cliche_' + name, text)
    p = run([t.compiler, '--emit-c', path])
    assert p.returncode == 1 and diagnostic in p.stderr, (
        name, p.returncode, p.stdout, p.stderr)
    assert not p.stdout, (name, 'partial C11 was published after rejection')


reject('unsatisfied', r'''
cliche Show { fn show(value: Self) -> string; }
fn main() { println(Show<bool>::show(true)); }
''', 'unsatisfied cliche obligation')

reject('missing_generic_bound', r'''
cliche Show { fn show(value: Self) -> string; }
fn render<T>(value: T) -> string { return Show<T>::show(value); }
fn main() {}
''', "add it to the function's where clause")

reject('unsatisfied_generic_call', r'''
cliche Show { fn show(value: Self) -> string; }
impl Show for i64 {
 fn show(value: i64) -> string { return "number"; }
}
fn render<T>(value: T) -> string where Show<T> {
 return Show<T>::show(value);
}
fn main() { println(render(true)); }
''', 'unsatisfied cliche obligation')

reject('signature', r'''
cliche Show { fn show(value: Self) -> string; }
impl Show for i64 {
 fn show(value: bool) -> string { return "wrong"; }
}
fn main() {}
''', 'signature mismatch for cliche member')

reject('parameter_mode', r'''
cliche Read { fn read(value: Self) -> i64; }
impl Read for i64 {
 fn read(value: ref i64) -> i64 { return value; }
}
fn main() {}
''', 'parameter modes differ')

reject('missing_member', r'''
cliche Pair {
 fn first(value: Self) -> i64;
 let second: i64;
}
impl Pair for i64 {
 fn first(value: i64) -> i64 { return value; }
}
fn main() {}
''', 'missing or extra members')

reject('duplicate_exact', r'''
cliche Mark { let value: i64; }
impl Mark for i64 { let value: i64 = 1; }
impl Mark for i64 { let value: i64 = 2; }
fn main() {}
''', 'duplicate cliche implementation head')

reject('overlap', r'''
record Pair<A,B> { first: A, second: B }
cliche Mark { let value: i64; }
impl<T> Mark for Pair<T,i64> { let value: i64 = 1; }
impl<U> Mark for Pair<i64,U> { let value: i64 = 2; }
fn main() {}
''', 'overlapping generic cliche implementations')

reject('recursive_prerequisite', r'''
record Box<T> { value: T }
cliche Mark { let value: i64; }
impl<T> Mark for Box<T> where Mark<Box<T>> {
 let value: i64 = 1;
}
fn main() {}
''', 'does not decrease structurally')

reject('member_arity', r'''
cliche Keep { fn keep<U>(value: Self, other: U) -> U; }
impl Keep for i64 {
 fn keep<U>(value: i64, other: U) -> U { return other; }
}
fn main() { println(Keep<i64>::keep(1, true)); }
''', 'wrong member type-argument arity')

reject('associated_value_call', r'''
cliche Default { let value: Self; }
impl Default for i64 { let value: i64 = 1; }
fn main() { println(Default<i64>::value()); }
''', 'cannot call value of type i64')

reject('generic_main', r'''
cliche Mark { let value: i64; }
fn main<T>() where Mark<T> {}
''', 'main cannot declare generic binders or cliche constraints')

reject('orphan', r'''
import cliche_library as Lib;
impl Lib.Imported for bool {
 fn add(value: bool, extra: i64) -> i64 { return extra; }
}
fn main() {}
''', 'orphan cliche implementation')

print('Core cliches: PASS (runtime, imports, deterministic C11 and 13 rejections)')
