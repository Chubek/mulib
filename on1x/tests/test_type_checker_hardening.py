#!/usr/bin/env python3
"""Source-to-C11 checks for hardened inference, type, member and owner paths."""
from support import NativeTests, run


t = NativeTests()

# Regression for the generic-context boundary: the retained local template is
# checked generically, while both executable instances remain concrete.
t.valid('type_checker_hardening_local_generic', '42\n', text=r'''
fn main() {
  let identity = fn(value) -> _ { return value; };
  println(identity(42));
  assert(identity(true));
}
''')

# One strict-C11 executable crosses all three adjacent integration paths. Static
# member selection must be complete before the unique owner reaches codegen.
t.valid('type_checker_hardening_members_owner', '42\n', text=r'''
record Box { value: i64 }
cliche Read { fn read(value: Self) -> i64; }
impl Read for Box {
  fn read(value: Box) -> i64 { return value["value"]; }
}
impl Box {
  fn make(value: i64) -> Box { return record<Box> { value: value }; }
}
fn main() {
  let owner = box(Box::make(42));
  let value = take(owner);
  println(Read<Box>::read(value));
}
''')


def reject(name, text, diagnostic):
    source = t.source('type_checker_hardening_' + name, text)
    output = t.work / (name + '.preserved.c')
    output.write_text('preserved output\n', encoding='utf-8')
    result = run([t.compiler, '--emit-c', source, '-o', output])
    assert result.returncode == 1 and diagnostic in result.stderr, (
        name, result.returncode, result.stdout, result.stderr)
    assert not result.stdout, (name, 'partial generated output on stdout')
    assert output.read_text(encoding='utf-8') == 'preserved output\n', (
        name, 'failed checking replaced the requested output')


reject('call_arity', r'''
fn identity(value: i64) -> i64 { return value; }
fn main() { identity(); }
''', 'wrong number of arguments')

reject('integer_form', r'''
fn main() { println(9223372036854775808); }
''', 'out of i64 range')

reject('forall_form', r'''
fn bad(value: forall(a) a) {}
fn main() {}
''', 'forall requires a fully specified function body')

reject('own_form', r'''
fn bad(value: own<i64>) {}
fn main() {}
''', 'own<T> requires a nominal record or enum type reference')

reject('unsatisfied_cliche', r'''
cliche Read { fn read(value: Self) -> i64; }
fn main() { println(Read<bool>::read(true)); }
''', 'unsatisfied cliche obligation')

reject('unknown_inherent_member', r'''
record Box { value: i64 }
impl Box { value answer: i64 = 42; }
fn main() { println(Box::missing); }
''', 'ON1X-IMPL-MEMBER')

reject('member_namespace_conflict', r'''
record Mark<T> { value: T }
cliche Mark { let code: i64; }
impl<T> Mark<T> { value code: i64 = 1; }
fn main() { println(Mark<i64>::code); }
''', "ambiguous '::' owner")

reject('use_after_move', r'''
record Box { value: i64 }
fn main() {
  let owner = box(record<Box> { value: 42 });
  drop(owner);
  drop(owner);
}
''', 'use after move')

print('type-checker hardening: strict C11, malformed forms, members and ownership PASS')
