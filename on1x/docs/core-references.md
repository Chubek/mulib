# Call-scoped `ref i64` parameters

```on1x
fn foo(k : ref i64) -> _ { k := 2; }

fn main() {
    let a = 4;
    foo(a);
    println(a);  # 2
}
```

`ref i64` is a parameter mode on a named On1x function, not a type of
first-class value. It takes an exclusive loan of the caller's named `i64`
place for the duration of the call. `k := e;` evaluates `e` as an `i64` and
writes to that place immediately. A bare `k` in a value position reads an `i64`
copy, so `return k;` in an `i64`-returning function returns a number, not the
reference. `let copy = k;` copies even when the original place is later changed.
Use a copy if a closure needs a snapshot of a reference parameter.

The caller may lend a local `let` or `var`, an addressable local binding
introduced by a pattern, or a by-value `i64` parameter. `let` still rejects direct
reassignment with `=`, and `:=` is only valid on a `ref i64` parameter. A call
to a known `ref i64` parameter is the only way to mutate a caller's `let` in
this profile. Immutable closure captures still take an `i64` snapshot at
creation, and a captured slot is not addressable through this facility.

The callee must be a direct named function (including an imported/aliased
On1x name). A reference argument must be a named place; literals, arithmetic,
call results, fields, and indices do not qualify. Argument evaluation is
left-to-right. Each exclusive loan starts at its argument and ends on
completion of the call. From the start of the loan, the same place cannot be
read, written, captured, or borrowed by the evaluation of later arguments.
For example, given compatible signatures, `f(a, a)` is legal when the first
argument copies `a` and the second lends `a`, but illegal when the first lends
`a` and the second reads or lends it. Distinct places can be lent together.
Binding identity follows lexical scope, not spelling.

A borrowing function may pass one of its parameters to another borrowing
function, including itself: the nested call temporarily acquires the same
exclusive access. The original loan resumes for the caller of that nested
call. The callee has no other path to the caller's local storage: references
cannot be copied as references, stored, returned, captured by closures,
retained by `defer`, or used as arguments to extern functions. First-class
functions with `ref` parameters, other referent types, and `ref` in ordinary
type annotations are not supported. This C11 profile rejects a unit that also
contains cooperative concurrency; it does not establish borrow safety across
suspension.

The inference engine checks the referent as `i64`; the parameter mode is tracked
separately from value types. Canonical HIR verification reconstructs lexical
places, checks reference stores, and verifies call-scoped loans independently
of source inference. The C11 emitter re-verifies the checked HIR and generates
private `int64_t *` parameters to initialized caller storage. It does not copy
back, heap-allocate or retain a reference, use C `restrict`, or expose a
pointer ABI. No runtime change is required. Terminal failures retain the
existing non-unwinding behavior. PPX Unit v1/`--emit-ast`, `.on1xi` interfaces,
scalar MIR and the independent Chisel/Nerotypical adapters do not represent
these borrows and reject them rather than treating them as scalar calls.

This feature does not itself admit recursive layouts. Both the
[shared nominal-owner profile](owning-adts.md) and the
[finite unique-owner profile](recursive-owning-adts.md) are independent of this
stack-scoped scalar borrow; neither turns `ref i64` into a general reference
type.

## Tests

Run from the MuLIB root with a configured compiler:

```sh
python3 on1x/tests/test_core_ref.py PATH/TO/on1xc on1x /tmp/opencode/core-ref-tests gcc
```

The runner tests real generated C11 with `-Wall -Wextra -Wpedantic -Werror`,
including inference, module aliases, recursive reborrows, simultaneous loans
to distinct places, evaluation order, closure snapshots, and rejection paths.
Pass `clang --cflag=-fsanitize=address,undefined` for a sanitized native run.
`on1x/tests/test_core_ref.cpp` additionally checks that malformed checked HIR
does not reach C11 emission.
