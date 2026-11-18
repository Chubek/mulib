#!/usr/bin/env python3
"""Enum connective: checked source patterns, PPX round-trip, and native C11."""
from support import NativeTests, run

t = NativeTests()
source = t.root / 'tests/fixtures/test_enum_connective.on1x'
expected = '3\n103\n3\n-3\n97\n42\n9\n7\n22\n20\n0\n9\n20\n2\n'
t.valid('enum_connective', expected, source=source)

# Identity PPX must preserve both the wildcard and the sum-only constraint.
generated, binary = t.work / 'enum_ppx.c', t.work / 'enum_ppx'
p = run([t.compiler, '--emit-c', '--stdppx', 'identity', source, '-o', generated])
assert p.returncode == 0, p.stderr
p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
         '-I', t.root / 'runtime', generated, t.root / 'runtime/on1x_runtime.c',
         *t.flags, '-o', binary])
assert p.returncode == 0, p.stderr
p = run([binary])
assert p.returncode == 0 and p.stdout == expected, (p.stdout, p.stderr)

t.source('enum_lib', '''
enum Answer { Yes(i64), No }
fn make(n:i64)->Answer { return variant<Answer>(Yes,n); }
''')
t.valid('enum_import', '42\n', text='''
import enum_lib as E;
fn main() {
  println(E.make(42) ~ { Yes(n) => n, No() => 0 });
}
''')


def rejected(name, text, error, *options):
    path = t.source(name, text)
    out = t.work / 'preserved.c'
    out.write_text('preserved\n')
    p = run([t.compiler, '--emit-c', *options, path, '-o', out])
    assert p.returncode == 1 and error in p.stderr, (name, p.returncode, p.stderr)
    assert f'{path}:2:' in p.stderr and not p.stdout, (name, p.stderr)
    assert out.read_text() == 'preserved\n'


nonsum = 'fn main() {\n let n = 1 ~ { - => 0 };\n}'
rejected('enum_non_sum', nonsum, 'requires a known closed sum')
rejected('enum_non_sum_ppx', nonsum, 'requires a known closed sum', '--stdppx', 'identity')
rejected('enum_non_sum_macro', '''
macro select { rule($value:Expression) => expression { $value ~ { - => 0 } }; }
fn main() { let n = select!(true); }
''', 'requires a known closed sum')
rejected('enum_unknown_subject', 'fn get(value)->i64 {\n return value ~ { - => 0 };\n}\nfn main(){}',
         'annotate the subject')
rejected('enum_plain_match_dash', 'fn main() {\n println(match 1 with { - => 0 });\n}',
         'expected a numeric pattern literal')
rejected('enum_arm_scope', 'enum E { C(i64) } fn main() {\n'
         ' variant<E>(C, 1) ~ { C(n) => println(n) }; println(n);\n}', 'undefined variable')

prefix = 'enum Result<T,E> { Ok(T), Err(E) } fn main() { let v=variant<Result<i64,i64>>(Ok,1);\n'
for name, arms, error in [
    ('empty', '', 'empty match'),
    ('missing_arm', 'Ok(-) => 1', 'non-exhaustive'),
    ('guard_only', 'Ok(n) if n > 0 => n, Err(-) => 0', 'guards do not prove coverage'),
    ('guard_type', 'Ok(n) if n => n, Ok(-) => 1, Err(-) => 0', 'expected bool'),
    ('unknown_constructor', 'No(-) => 1, - => 0', 'unknown constructor'),
    ('payload_arity', 'Ok(-, -) => 1, Err(-) => 0', 'payload arity'),
    ('bad_dash', 'Ok(-true) => 1, - => 0', 'negative pattern literal must be numeric'),
    ('redundant', 'Ok(-) => 1, Ok(1) => 2, Err(-) => 0', 'redundant'),
    ('arm_type', 'Ok(-) => 1, Err(-) => false', 'expected'),
]:
    rejected('enum_' + name, prefix + ' let n=v ~ { ' + arms + ' };\n}', error)

print('Enum connective: native C11, nominal and structural sums, guard order, and diagnostics PASS')
