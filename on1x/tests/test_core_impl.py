#!/usr/bin/env python3
"""Intrinsic nominal impl blocks: static dispatch, coherence and boundaries."""
from support import NativeTests, run


t = NativeTests()

expected = (
    '42\n5\ntrue\n7\ntrue\n'
    '1\n2\n42\n1\n40\n11\n9\ntrue\n8\n6\n6\n13\n'
)
t.valid('core_impl', expected)

# Publication is deterministic and contains direct private functions, not a
# runtime implementation-name registry.
example = t.root / 'examples/core_impl.on1x'
first, second = t.work / 'core_impl-first.c', t.work / 'core_impl-second.c'
for output in (first, second):
    p = run([t.compiler, '--emit-c', example, '-o', output])
    assert p.returncode == 0 and not p.stdout, p.stderr
generated = first.read_text(encoding='utf-8')
assert generated == second.read_text(encoding='utf-8')
assert 'on1x_fn_inherent_' in generated and 'strcmp(' not in generated

# Values are calls, not memoized globals, and member bodies use the ordinary
# hygienic macro expansion pipeline after declaration preparation.
t.valid('core_impl_values_and_macros', '3\n4\n3\n4\n42\n42\n', text=r'''
macro plus_one {
  rule($value:Expression) => expression { $value + 1 };
}
record Marker { value: i64 }
fn computed() -> i64 { println(3); return 4; }
impl Marker {
  value current: i64 = computed();
  fn answer() -> i64 { return plus_one!(41); }
  fn increment(self) -> i64 { return plus_one!(self["value"]); }
}
fn main() {
  println(Marker::current);
  println(Marker::current);
  println(Marker::answer());
  let marker = record<Marker>{value:41};
  println(marker::increment());
}
''')

# Cliche and inherent implementations share the generic binder/type carriers,
# but remain separate deterministic `::` namespaces.
t.valid('core_impl_with_cliche', '41\n42\n', text=r'''
macro plus_one {
  rule($value:Expression) => expression { $value + 1 };
}
record Box<T> { value: T }
cliche Increment {
  fn increment(value: Self) -> i64;
}
impl Increment for i64 {
  fn increment(value: i64) -> i64 { return plus_one!(value); }
}
impl<T> Box<T> {
  fn choose<U>(left: T, right: U) -> U { return right; }
  value answer: i64 = 42;
}
fn main() {
  println(Increment<i64>::increment(40));
  println(Box<bool>::choose(false,Box<bool>::answer));
}
''')


def native_c(name, source):
    exe = t.work / name
    p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
             '-I', t.root / 'runtime', source, t.root / 'runtime/on1x_runtime.c',
             *t.flags, '-o', exe])
    assert p.returncode == 0, (name, p.stderr)
    return exe


# Static scalar members elaborate to ordinary scalar functions, so every
# existing scalar MIR path preserves their behavior.
scalar = t.source('core_impl_scalar_mir', r'''
record Marker { unused: i64 }
impl Marker {
  fn plus(left: i64, right: i64) -> i64 { return left + right; }
  value answer: i64 = 42;
}
fn main() { println(Marker::plus(Marker::answer,0)); }
''')
outputs = []
for suffix, options in (
        ('hir', ['--emit-c']),
        ('mir', ['--emit-ir-c']),
        ('mir-opt', ['--emit-ir-c', '--optimize-ir'])):
    c_path = t.work / ('core_impl_scalar_' + suffix + '.c')
    p = run([t.compiler, *options, scalar, '-o', c_path])
    assert p.returncode == 0, (suffix, p.stderr)
    p = run([native_c('core_impl_scalar_' + suffix, c_path)])
    assert p.returncode == 0, (suffix, p.stderr)
    outputs.append(p.stdout)
for options in ([], ['--optimize-ir']):
    p = run([t.compiler, '--run-ir', scalar, *options])
    assert p.returncode == 0, p.stderr
    outputs.append(p.stdout)
assert outputs == ['42\n'] * len(outputs), outputs
one = run([t.compiler, '--emit-ir', scalar])
two = run([t.compiler, '--emit-ir', scalar])
assert one.returncode == two.returncode == 0 and one.stdout == two.stdout

# Canonical nominal identity selects members across qualified imports. Equal
# layouts from separate modules remain distinct, and aliases retain identity.
t.source('core_impl_left', r'''
record Token { value: i64 }
impl Token {
  fn make(value: i64) -> Token { return record<Token>{value:value}; }
  fn read(self) -> i64 { return self["value"]; }
  value code: i64 = 1;
}
''')
t.source('core_impl_right', r'''
record Token { value: i64 }
impl Token {
  fn make(value: i64) -> Token { return record<Token>{value:value}; }
  fn read(self) -> i64 { return self["value"]; }
  value code: i64 = 2;
}
''')
t.valid('core_impl_imports', '1\n2\n11\n22\n', text=r'''
import core_impl_left as L, core_impl_right as R;
fn main() {
  println(L.Token::code);
  println(R.Token::code);
  println(L.Token::make(11)::read());
  println(R.Token::make(22)::read());
}
''')
t.source('core_impl_reexport', r'''
from core_impl_left import Token as Item;
''')
t.valid('core_impl_reexport_use', '1\n31\n', text=r'''
from core_impl_reexport import Item;
fn main() {
  println(Item::code);
  println(Item::make(31)::read());
}
''')


def reject(name, text, expected_text, *options):
    path = t.source('reject_core_impl_' + name, text)
    output = t.work / (name + '.rejected.c')
    output.write_text('preserved output\n', encoding='utf-8')
    p = run([t.compiler, '--emit-c', *options, path, '-o', output])
    assert p.returncode == 1 and expected_text in p.stderr, (
        name, p.returncode, p.stdout, p.stderr)
    assert not p.stdout, (name, 'partial C11 was published')
    assert output.read_text(encoding='utf-8') == 'preserved output\n'
    return p


reject('unknown_member', r'''
record Box { value: i64 }
impl Box { value answer: i64 = 42; }
fn main() { println(Box::missing); }
''', 'ON1X-IMPL-MEMBER')

reject('method_requires_receiver', r'''
record Box { value: i64 }
impl Box { fn get(self) -> i64 { return self["value"]; } }
fn main() { println(Box::get()); }
''', 'requires a receiver')

reject('associated_rejects_receiver', r'''
record Box { value: i64 }
impl Box {
  fn make(value: i64) -> Box { return record<Box>{value:value}; }
}
fn main() { let box=record<Box>{value:1}; box::make(2); }
''', 'does not accept a receiver')

reject('receiver_method_first_class', r'''
record Box { value: i64 }
impl Box { fn get(self) -> i64 { return self["value"]; } }
fn main() { let box=record<Box>{value:1}; let get=box::get; }
''', 'must be called with an argument list')

reject('value_is_not_callable', r'''
record Box { value: i64 }
impl Box { value answer: i64 = 42; }
fn main() { println(Box::answer()); }
''', 'is not callable')

# Member names are unique per canonical nominal, regardless of target arguments.
reject('duplicate_across_arguments', r'''
record Box<T> { value: T }
impl Box<i64> { value code: i64 = 1; }
impl Box<bool> { fn code() -> i64 { return 2; } }
fn main() {}
''', 'ON1X-IMPL-DUPLICATE')

reject('borrow_receiver', r'''
record Box { value: i64 }
impl Box { fn get(ref self) -> i64 { return self["value"]; } }
fn main() {}
''', 'ON1X-IMPL-RECEIVER')

reject('move_receiver', r'''
record Box { value: i64 }
impl Box { fn get(move self) -> i64 { return self["value"]; } }
fn main() {}
''', 'ON1X-IMPL-RECEIVER')

# A generic declaration can exist, but a concrete receiver is rechecked after
# specialization. Managed/string layouts cannot silently acquire by-value copies.
reject('unsafe_concrete_receiver', r'''
record Box<T> { value: T }
impl<T> Box<T> {
  fn make(value: T) -> Box<T> { return record<Box<T>>{value:value}; }
  fn get(self) -> T { return self["value"]; }
}
fn main() { let box=Box<string>::make("managed"); println(box::get()); }
''', 'ON1X-IMPL-RECEIVER')

reject('unsafe_shared_receiver', r'''
record Shared { value: sexpr }
impl Shared { fn get(self) -> sexpr { return self["value"]; } }
fn main() {}
''', 'ON1X-IMPL-RECEIVER')

reject('undetermined_parameter', r'''
record Pair<A,B> { first: A, second: B }
impl<T,U> Pair<T,i64> { value code: i64 = 1; }
fn main() {}
''', 'not determined by the target')

reject('wrong_self_annotation', r'''
record A { value: i64 }
record B { value: i64 }
impl A { fn get(self: B) -> i64 { return self["value"]; } }
fn main() {}
''', 'self annotation must exactly match')

reject('explicit_member_arguments', r'''
record Box<T> { value: T }
impl<T> Box<T> {
  fn choose<U>(left: T, right: U) -> U { return right; }
}
fn main() { println(Box<i64>::choose<bool>(1,true)); }
''', 'explicit inherent member type arguments are not supported')

reject('ambiguous_namespace', r'''
record Mark<T> { value: T }
cliche Mark { let code: i64; }
impl<T> Mark<T> { value code: i64 = 1; }
fn main() { println(Mark<i64>::code); }
''', "ambiguous '::' owner")

# Inherent implementations are orphan-free: the declaration and block belong
# to the same source module.
reject('orphan', r'''
from core_impl_left import Token;
impl Token { value local: i64 = 9; }
fn main() {}
''', 'ON1X-IMPL-COHERENCE')

# Unit-v1 PPX, interface transport and the restricted KernelBind profile do not
# erase or reinterpret host-retained implementation metadata.
boundary = r'''
record Marker { value: i64 }
impl Marker { value answer: i64 = 42; }
fn answer() -> i64 { return Marker::answer; }
fn main() { println(answer()); }
'''
reject('ppx_boundary', boundary, 'ON1X-IMPL-UNSUPPORTED',
       '--stdppx', 'identity')
reject('kernel_boundary', boundary, 'ON1X-KERNEL-PROFILE',
       '--kernel-c11')

interface_source = t.source('core_impl_interface', boundary)
interface = t.work / 'core_impl_interface.on1xi'
interface.write_text('on1xi 1;\nfn answer() -> i64;\n', encoding='utf-8')
p = run([t.compiler, '--no-prelude', '--check-interfaces', interface_source,
         '--interface', interface])
assert p.returncode == 1 and 'does not transport intrinsic impl' in p.stderr
assert not p.stdout

# Aggregate receivers are outside scalar MIR; this must be a located explicit
# backend rejection rather than a semantic fallback.
for mode in ('--emit-ir', '--emit-ir-c', '--run-ir'):
    p = run([t.compiler, mode, example])
    assert p.returncode == 1 and 'ON1X-MIR-UNSUPPORTED' in p.stderr, (
        mode, p.stdout, p.stderr)
    assert not p.stdout

# Parsing and preparation have independent finite limits.
blocks = ''.join(
    f'impl Marker{{value member{i}:i64={i};}}\n' for i in range(1025))
reject('implementation_limit',
       'record Marker{value:i64}\n' + blocks + 'fn main(){}\n',
       'inherent implementation limit exceeded')

print('Core impl: PASS (C11/MIR, generics, modules, ownership and boundaries)')
