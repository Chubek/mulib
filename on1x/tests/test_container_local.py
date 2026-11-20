#!/usr/bin/env python3
"""Compile and execute the checked local container slice through strict C11."""
import os

from support import NativeTests, run


t = NativeTests()
fixtures = t.root / 'tests/fixtures'
allocator = ('-include', fixtures / 'unique_allocator.h',
             fixtures / 'unique_allocator.c')
expected = 'container local lifecycle\n'
exe = t.build('container_local_lifecycle',
              source=fixtures / 'container_local/lifecycle.on1x', extra=allocator)
env = os.environ.copy()
env.pop('ON1X_TEST_UNIQUE_FAIL_AFTER', None)
result = run([exe], env=env)
assert result.returncode == 0 and result.stdout == expected and not result.stderr, (
    result.returncode, result.stdout, result.stderr)

# Every box allocation in the lifecycle must fail terminally, not leak, abort
# due to the allocator's atexit live-count check, or publish the success line.
allocations = 9
for fail_after in range(allocations):
    result = run([exe], env={**env, 'ON1X_TEST_UNIQUE_FAIL_AFTER': str(fail_after)})
    assert result.returncode == 72 and not result.stdout, (
        fail_after, result.returncode, result.stdout, result.stderr)
    assert 'owning-box allocation failure' in result.stderr, result.stderr
result = run([exe], env={**env, 'ON1X_TEST_UNIQUE_FAIL_AFTER': str(allocations)})
assert result.returncode == 0 and result.stdout == expected and not result.stderr, (
    result.returncode, result.stdout, result.stderr)

imports = '''
import std.container.Box as BoxModule;
import std.container.Cell as CellModule;
import std.container.Local as LocalModule;
import std.container.Quarantine as Quarantine;
'''


def reject(name, text, diagnostic):
    src = t.source('container_local_reject_' + name, imports + text)
    result = run([t.compiler, '--emit-c', src])
    assert result.returncode == 1 and not result.stdout, (
        name, result.returncode, result.stdout, result.stderr)
    assert diagnostic in result.stderr and ': error:' in result.stderr, (
        name, diagnostic, result.stderr)


reject('copy_box', '''
fn main() {
    let owner = BoxModule.basis_box_new(4);
    let duplicate = owner;
}
''', 'copy of unique local requires move(local)')

reject('take_borrowed_box', '''
fn main() {
    let owner = BoxModule.basis_box_new(4);
    let loan = borrow owner;
    BoxModule.basis_box_into_inner(move(owner));
}
''', 'cannot consume')

reject('after_take', '''
fn main() {
    let owner = BoxModule.basis_box_new(4);
    BoxModule.basis_box_into_inner(move(owner));
    println(BoxModule.basis_box_get(owner));
}
''', 'moved')

reject('copy_move_only_payload', '''
fn main() {
    let child = BoxModule.basis_box_new(4);
    let owner = box(record<BoxModule.Box<own<BoxModule.Box<i64>>>> { value: move(child) });
    drop(BoxModule.basis_box_get(owner));
}
''', 'copy of unique record field')

reject('unsupported_box_string', '''
fn main() {
    let owner = BoxModule.basis_box_new("not a nominal payload");
}
''', 'unsupported string')

reject('alias_cell_swap', '''
fn main() {
    let cell = CellModule.basis_cell_new(true);
    CellModule.basis_cell_swap(cell, cell);
}
''', 'overlapping exclusive')

reject('write_shared_cell', '''
fn main() {
    let cell = CellModule.basis_cell_new(true);
    let loan = borrow cell;
    CellModule.basis_cell_set(cell, false);
}
''', 'overlapping exclusive')

reject('local_capture', '''
fn inspect(local: borrow LocalModule.Local<i64>) -> fn() -> i64 {
    return fn() -> i64 {
        return option_unwrap_or(LocalModule.basis_local_peek(local), 0);
    };
}
fn main() {}
''', 'capture')

reject('quarantine_missing_policy', '''
record Unchecked { enabled: bool }
fn main() {
    let staged = Quarantine.basis_quarantine_stage(17);
    Quarantine.basis_quarantine_review(move(staged), record<Unchecked>{enabled:true});
}
''', 'unsatisfied cliche obligation')

reject('quarantine_wrong_payload', '''
fn main() {
    let staged = Quarantine.basis_quarantine_stage(false);
    Quarantine.basis_quarantine_check(staged,
        record<Quarantine.BasisQuarantineI64Range>{low:0,high:1});
}
''', 'unsatisfied cliche obligation')

reject('quarantine_reuse', '''
fn main() {
    let staged = Quarantine.basis_quarantine_stage(1);
    let range = record<Quarantine.BasisQuarantineI64Range>{low:0,high:1};
    Quarantine.basis_quarantine_review(move(staged), range);
    Quarantine.basis_quarantine_check(staged, range);
}
''', 'moved')

# Local/Quarantine use the exact common nominal vocabulary, even when the
# automatic prelude is disabled and the source loads no Option/Result helpers.
explicit = t.source('container_local_explicit_types', imports + '''
fn main() {
    let local: LocalModule.Local<i64> = LocalModule.basis_local_empty();
    let slot: LocalModule.Option<i64> = LocalModule.basis_local_peek(local);
    let result: Quarantine.Result<i64,own<Quarantine.Quarantined<i64>>> =
        Quarantine.basis_quarantine_review(Quarantine.basis_quarantine_stage(2),
            record<Quarantine.BasisQuarantineI64Range>{low:1,high:3});
}
''')
result = run([t.compiler, '--no-prelude', '--emit-c', explicit])
assert result.returncode == 0 and result.stdout, result.stderr

# A shared wrapper retains the same graph without allocating another node.
# A return and lexical drops must balance the references independently.
t.valid('container_rc_snapshot', '', text='''
import std.container.Rc as R;
fn build() -> R.RcSExpr {
  let initial = R.basis_rc_sexpr_new(sx_string("shared"));
  let shared = R.basis_rc_sexpr_clone(initial);
  assert(data_live_nodes() == 1);
  return shared;
}
fn main() {
  assert(data_live_nodes() == 0);
  {
    let owner = build();
    let node = R.basis_rc_sexpr_get(owner);
    assert(sx_text(node) == "shared" && data_live_nodes() == 1);
  }
  assert(data_live_nodes() == 0);
}
''', extra=('-include', fixtures/'closure_allocator.h', fixtures/'closure_allocator.c'))
print(f'container local: unique/shared lifecycles, {allocations} allocation faults, 11 compile rejections')
