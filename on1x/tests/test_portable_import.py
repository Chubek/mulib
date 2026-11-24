#!/usr/bin/env python3
"""Portable import syntax, lexical resolution, and legacy build separation."""
import json
import sys

from support import NativeTests, run

t = NativeTests()


def compile_source(mode, source, *flags):
    return run([t.compiler, '--no-prelude', mode, *flags, source])


def good(mode, source, *flags):
    p = compile_source(mode, source, *flags)
    assert p.returncode == 0, p
    return p.stdout


def bad(name, text, diagnostic, *flags):
    source = t.source(name, text)
    for mode in ('--check', '--emit-c', '--emit-ir'):
        p = compile_source(mode, source, *flags)
        assert p.returncode == 1 and diagnostic in p.stderr and not p.stdout, p
        assert f'{source.resolve()}:' in p.stderr, p


# `foreign` is still an ordinary module/identifier outside the new production.
t.source('foreign', 'fn answer()->i64 {return 40;}')
(t.work / 'foreign').mkdir(exist_ok=True)
(t.work / 'foreign/value.on1x').write_text('fn answer()->i64 {return 2;}', encoding='utf-8')
ordinary = t.source('ordinary_foreign', '''\
import foreign, foreign.value as Child;
fn main(){let answer=foreign.answer();println(answer+Child.answer());}
''')
assert good('--run-ir', ordinary) == '42\n'

# One backing function across multiple aliases and source modules. A private
# import in a child neither leaks into its caller nor yields a native request.
child = t.source('portable_child', '''\
import foreign stdffi.clamp as ChildClamp;
fn value()->i64 {return ChildClamp.add_i64(39, 1);}
''')
entry = t.source('portable_entry', '''\
import portable_child as Child;
import foreign stdffi.clamp;
import foreign stdffi.clamp as Other;
fn main(){
  let base = Child.value();
  { let clamp = 1; println(clamp); }
  println(clamp.add_i64(base, Other.add_i64(1,1)));
}
''')
assert good('--check', entry) == 'OK: 3 functions\n'
assert good('--run-ir', entry) == '1\n42\n'
dependency_paths = good('--deps', entry).splitlines()
assert dependency_paths == list(map(str, (child.resolve(), entry.resolve()))), dependency_paths
assert json.loads(good('--kernel-imports', entry))['requests'] == []

quoted = t.source('portable_quoted', '''\
import "portable_child.on1x";
import foreign stdffi.clamp as ChildClamp;
fn main(){println(ChildClamp.add_i64(value(),2));}
''')
assert good('--check', quoted) == 'OK: 3 functions\n'
assert good('--run-ir', quoted) == '42\n'

# A portable import needs no headers, ABI review, or --kernel-bind when
# compiled by the ordinary build driver.
product = t.work / 'portable_driver'
p = run([sys.executable, t.root / 'tools/on1x-build.py',
         entry, '--on1xc', t.compiler, '--cc', t.cc, '--no-prelude', '--via-mir', '-o', product,
         *[f'--cflag={flag}' for flag in t.flags]])
assert p.returncode == 0, p
p = run([product])
assert p.returncode == 0 and p.stdout == '1\n42\n', p

unused = t.source('portable_unused', 'import foreign stdffi.clamp; fn main(){}')
assert good('--check', unused) == 'OK: 1 functions\n'

# An admitted export is an ordinary function value, not an extern or a
# capability. A local binder still shadows the module alias lexically.
first_class = t.source('portable_first_class', '''\
import foreign stdffi.clamp as foreign;
fn main(){let f=foreign.add_i64; println(f(40,2));}
''')
assert good('--check', first_class) == 'OK: 2 functions\n'

portable = 'import foreign stdffi.clamp as C;\n'
for name, text, diagnostic in (
    ('unknown_kernel', 'import foreign stdffi.does_not_exist; fn main(){}', 'ON1X-STDFFI-KERNEL'),
    ('unknown_member', portable+'fn main(){C.does_not_exist();}', 'ON1X-STDFFI-MEMBER'),
    ('unknown_value', portable+'fn main(){let f=C.does_not_exist;}', 'ON1X-STDFFI-MEMBER'),
    ('type_member', portable+'fn main(){let x:C.I64=0;}', 'ON1X-STDFFI-MEMBER'),
    ('bad_call_arity', portable+'fn main(){C.add_i64(1);}', 'wrong number of arguments'),
    ('bad_call_argument', portable+'fn main(){C.add_i64(1,true);}', 'expected i64, got bool'),
    ('bare_alias', portable+'fn main(){let x=C;}', 'not a first-class value'),
    ('local_shadow', portable+'fn main(){let C=1; C.add_i64(1,2);}', 'shadowed by local binding'),
    ('param_shadow', portable+'fn hidden(C:i64){C.i64(1,2,3);} fn main(){}', 'shadowed by local binding'),
    ('function_collision', portable+'fn C(){} fn main(){}', 'ON1X-STDFFI-ALIAS'),
    ('module_collision', portable+'import portable_child as C; fn main(){}', 'ON1X-STDFFI-ALIAS'),
    ('duplicate_alias', portable+portable+'fn main(){}', 'ON1X-STDFFI-ALIAS'),
    ('missing_scope', 'import portable_child; fn main(){ChildClamp.add_i64(1,2);}', 'unknown module qualifier'),
    ('quoted_scope', 'import "portable_child.on1x"; fn main(){ChildClamp.add_i64(1,2);}', 'unknown module qualifier'),
    ('no_reexport', 'import portable_child; fn main(){portable_child.ChildClamp.add_i64(1,2);}', 'module has no export'),
    ('unsafe_migration', 'unsafe import foreign stdffi.clamp; fn main(){}', 'use import foreign stdffi.<kernel>'),
    ('nonstandard_prefix', 'import foreign mulib.clamp; fn main(){}', "expected 'stdffi'"),
    ('linkage', 'import foreign[dynlib] stdffi.clamp; fn main(){}', "expected 'stdffi'"),
    ('qualified_kernel', 'import foreign stdffi.clamp.extra; fn main(){}', "expected ';'"),
    ('no_member', 'import foreign stdffi.; fn main(){}', 'expected identifier'),
    ('no_alias', 'import foreign stdffi.clamp as; fn main(){}', 'expected identifier'),
):
    bad(name, text, diagnostic)

native = 'unsafe import kernel mulib.clamp as Native;\n'
mixed = t.source('mixed', portable+native+'''\
fn main(){unsafe { println(C.add_i64(40,Native.add_i64(1,1))); }}
''')
requests = json.loads(good('--kernel-imports', mixed))['requests']
assert [(r['module'], r['alias'], r['unsafe']) for r in requests] == [('mulib.clamp', 'Native', True)]
assert compile_source('--check', mixed).returncode == 1
p = compile_source('--emit-c', mixed, '--kernel-c11')
if p.returncode == 0:
    assert 'unsafe_foreign: mulib_clamp_add_i64' in p.stdout, p
else:
    # Direct C11 can be built without the optional Cephyr kernel runtime.
    assert p.returncode == 1 and 'requires the Cephyr kernel runtime' in p.stderr and not p.stdout, p

mixed_collision = t.source('mixed_collision', portable+native.replace('Native', 'C')+'fn main(){}')
for mode, flags in (('--kernel-imports', ()), ('--emit-c', ('--kernel-c11',))):
    p = compile_source(mode, mixed_collision, *flags)
    assert p.returncode == 1 and 'ON1X-STDFFI-ALIAS' in p.stderr and not p.stdout, p
