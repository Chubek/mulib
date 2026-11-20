#!/usr/bin/env python3
"""Checked shared containers, generated C11, allocation/spawn failures and stress."""
import json
import os
import sys

from support import NativeTests, run


t = NativeTests()
fixtures = t.root / 'tests/fixtures'
shared = fixtures / 'container_shared'
container = t.root / 'stdlib/container'
flags = ('-D_POSIX_C_SOURCE=200809L', '-pthread', '-I', container,
         '-I', t.root.parent / 'include')
extra = (*flags, container / 'shared.c', shared / 'kernels.c')
alloc = ('-include', fixtures / 'unique_allocator.h', fixtures / 'unique_allocator.c',
         '-DON1X_SHARED_MALLOC=on1x_test_unique_malloc',
         '-DON1X_SHARED_FREE=on1x_test_unique_free')
env = os.environ.copy()
env.pop('ON1X_TEST_UNIQUE_FAIL_AFTER', None)
env.pop('ON1X_TEST_SHARED_THREAD_FAIL_AFTER', None)
exe = t.build('container_shared_lifecycle', source=shared / 'lifecycle.on1x',
              extra=(*extra, *alloc))
result = run([exe], env=env)
assert (result.returncode, result.stdout, result.stderr) == (0, 'shared containers\n', ''), result

# Fault both mallocs of new and the claim allocation of clone, including
# retained S-expressions, nested finite owners, and unrelated live claims.
allocations = 19
for n in range(allocations):
    result = run([exe], env={**env, 'ON1X_TEST_UNIQUE_FAIL_AFTER': str(n)})
    assert result.returncode == 72 and not result.stdout and (
        'owning-box allocation failure' in result.stderr), (n, result)
result = run([exe], env={**env, 'ON1X_TEST_UNIQUE_FAIL_AFTER': str(allocations)})
assert (result.returncode, result.stdout, result.stderr) == (0, 'shared containers\n', ''), result
dynamic = t.build('container_shared_refcell', source=shared / 'refcell.on1x',
                  extra=(*extra, *alloc))
dynamic_allocations = 7
for n in range(dynamic_allocations + 1):
    result = run([dynamic], env={**env, 'ON1X_TEST_UNIQUE_FAIL_AFTER': str(n)})
    if n == dynamic_allocations:
        assert (result.returncode, result.stdout, result.stderr) == (0, 'dynamic borrows\n', ''), result
    else:
        assert result.returncode == 72 and not result.stdout and (
            'owning-box allocation failure' in result.stderr), (n, result)

imports = '''
import std.container.Arc as A;
import std.container.Rc as R;
import std.container.RefCell as C;
'''
fault = t.build('container_shared_thread_fault', text=imports + '''
fn main() {
    let owner = A.basis_arc_new(array[0u8, 1u8, 0u8, 255u8]);
    println(A.basis_arc_parallel_count(owner, 0u8, 4));
    assert(A.basis_arc_get(owner)[3] == 255u8);
}
''', extra=(*extra, *alloc, '-include', shared / 'thread_fault.h', shared / 'thread_fault.c'))
for n in range(5):
    result = run([fault], env={**env, 'ON1X_TEST_SHARED_THREAD_FAIL_AFTER': str(n)})
    assert (result.returncode, result.stdout, result.stderr) == (
        0, '2\n' if n == 4 else '-1\n', ''), (n, result)


def reject(name, source, diagnostic, flags=()):
    path = t.source('container_shared_reject_' + name, imports + source)
    result = run([t.compiler, *flags, '--emit-c', path])
    assert result.returncode == 1 and not result.stdout, (name, result)
    assert 'error:' in result.stderr and diagnostic in result.stderr, (name, result.stderr)


reject('copy', 'fn main() { let a = R.basis_rc_new(1); let b = a; }', 'copy of unique')
reject('double_drop', 'fn main() { let a = A.basis_arc_new(1); drop(a); drop(a); }', 'move')
reject('read_moved', 'fn main() { let a = R.basis_rc_new(1); drop(a); R.basis_rc_get(a); }',
       'moved')
reject('direct_borrow', 'fn main() { let a = R.basis_rc_new(1); let b = borrow a; }',
       'opaque shared owner')
reject('direct_take', 'fn main() { let a = R.basis_rc_new(1); take(a); }', 'opaque shared owner')
reject('forge', 'fn main() { let a = box(record<A.Arc<i64>>{}); }', 'opaque shared owner')
reject('rc_as_arc', 'fn main() { let a = R.basis_rc_new(1); A.basis_arc_clone(a); }',
       'Arc')
reject('arc_managed', 'fn main() { let a = A.basis_arc_new(sx_string("local")); }',
       'pointer-free')
reject('arc_unique', 'record Box { n:i64 } fn main() { let a = A.basis_arc_new(box(record<Box>{n:1})); }',
       'pointer-free')
reject('rc_string', 'fn main() { let a = R.basis_rc_new("not a managed payload"); }',
       'unsupported Rc payload')
reject('copy_unique_payload', '''
record Box { n:i64 } fn main() {
    let a = R.basis_rc_new(box(record<Box>{n:1}));
    let b = R.basis_rc_get(a);
}
''', 'cannot be copied')
reject('not_byte_array', 'fn main() { let a = A.basis_arc_new(1); A.basis_arc_parallel_count(a,0u8,1); }',
       'requires Arc<array<u8,N>>')
reject('capture', '''
fn main() { let a = A.basis_arc_new(1); let f = fn() -> i64 { return A.basis_arc_get(a); }; }
''', 'capture unique')
reject('channel_payload', '''
fn main() {
    taskgroup {
        let pipe = ->(1, 1) { payload: own<A.Arc<i64>>, capacity: 2, mode: uni };
    }
}
''', 'ON1X-CSP-UNSUPPORTED')
reject('csp_unit', '''
fn main() {
    let owner = A.basis_arc_new(1);
    taskgroup { assert(true); }
}
''', 'unique ownership cannot cross cooperative concurrency')
reject('loan_conflict', '''
fn consume(owner: own<A.Arc<array<u8,1>>>) -> i64 { return 1; }
fn main() {
    let a = A.basis_arc_new(array[0u8]);
    A.basis_arc_parallel_count(a, 0u8, consume(move(a)));
}
''', 'borrow')
reject('forge_by_same_spelling', '''
record Arc<T> {}
fn main() { let fake = box(record<Arc<i64>>{}); A.basis_arc_clone(fake); }
''', 'Arc')
reject('refcell_forge', 'fn main() { let a = box(record<C.RefMut<i64>>{}); }',
       'opaque shared owner')
reject('replace_with_reader', '''
fn main() {
    let a = C.basis_refcell_new(1);
    match C.basis_refcell_try_borrow(a) with {
        Busy() => assert(false),
        Granted(reader) => assert(C.basis_refcell_replace(reader, 2) == 1)
    };
}
''', 'RefMut')
reject('copy_guard', '''
fn main() {
    let a = C.basis_refcell_new(1);
    match C.basis_refcell_try_borrow(a) with {
        Busy() => assert(false),
        Granted(reader) => { let alias = reader; }
    };
}
''', 'copy of unique')
reject('direct_cell_read', '''
fn main() { let a = C.basis_refcell_new(1); C.basis_refcell_read(a); }
''', 'Ref')
reject('guard_after_drop', '''
fn main() {
    let a = C.basis_refcell_new(1);
    match C.basis_refcell_try_borrow(a) with {
        Busy() => assert(false),
        Granted(reader) => { drop(reader); assert(C.basis_refcell_read(reader) == 1); }
    };
}
''', 'moved')
reject('guard_exclusive_call_loan', '''
fn main() {
    let a = C.basis_refcell_new(1);
    match C.basis_refcell_try_borrow_mut(a) with {
        Busy() => assert(false),
        Granted(writer) => {
            C.basis_refcell_replace(writer, C.basis_refcell_read_mut(writer) + 1);
        }
    };
}
''', 'borrow')

result = run([t.compiler, '--emit-ir', shared / 'lifecycle.on1x'])
assert result.returncode == 1 and not result.stdout and (
    'ON1X-MIR-UNSUPPORTED' in result.stderr), result
no_prelude = t.source('container_shared_no_prelude', imports + '''
fn main() {
    let owner: own<R.Rc<i64>> = R.basis_rc_new(7);
    assert(R.basis_rc_get(owner) == 7);
}
''')
result = run([t.compiler, '--no-prelude', '--emit-c', no_prelude])
assert result.returncode == 0 and 'on1x_shared_v1_new' in result.stdout, result

stress = t.work / 'container_shared_stress'
result = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
              *flags, '-I', t.root / 'runtime', t.root / 'tests/test_container_shared_stress.c',
              t.root / 'runtime/on1x_runtime.c', shared / 'kernels.c',
              *t.flags, '-o', stress])
assert result.returncode == 0, result.stderr
result = run([stress], env=env)
assert (result.returncode, result.stdout, result.stderr) == (0, '', ''), result

# The normal build driver deduplicates MuLIB's xthreads switch when both
# the shared containers and the older TLS adapter are used in one program.
driver_exe, plan_path = t.work / 'container_shared_driver', t.work / 'container_shared_plan.json'
result = run([sys.executable, t.root / 'tools/on1x-build.py', shared / 'driver.on1x',
              '--on1xc', t.compiler, '--cc', t.cc, '--mulib-root', t.root.parent,
              *['--cflag=' + flag for flag in t.flags],
              '--mulib-plan', plan_path, '-o', driver_exe])
assert result.returncode == 0, result
result = run([driver_exe], env=env)
assert (result.returncode, result.stdout, result.stderr) == (0, '2\n', ''), result
plan = json.loads(plan_path.read_text())
assert plan['safe_export'] is False
assert plan['container_bridge_groups'] == ['SHARED', 'TLS']
assert plan['plan']['implementation_translation_units'] == 1
assert sorted(plan['plan']['implementation_macros']) == ['MULIB_IMPL_XATOMICS', 'MULIB_IMPL_XTHREADS']
assert all(str((container / name).resolve()) in plan['inputs']
           for name in ('shared.c', 'on1x_container_shared.h'))
print(f'shared containers: {allocations + dynamic_allocations} allocation faults, four launch faults, checked ownership, dynamic borrows and native stress')
