# Checked local containers

These modules use the ordinary On1x compiler and its checked, direct C11
backend. They need no native adapter, raw handles, or additional build flags.
This is a synchronous, caller-owned storage API, not a concurrent one.

| Import | Operations (all names have the `basis_` prefix) |
| --- | --- |
| `std.container.Box` | `box_new` allocates an `own<Box<T>>`; `box_get` copies the payload through a shared loan; `box_replace` writes through an exclusive loan and returns the previous value; `box_exchange_box` transfers a replacement inner `own<Box<T>>` into a nested box and returns the previous owner without reallocating the outer box; `box_into_inner` frees the box and returns its payload. An unconsumed owner is dropped by the compiler. |
| `std.container.Cell` | `cell_new` creates a `Cell<T>` value; `cell_get` reads through a shared loan; `cell_set` and `cell_replace` need an exclusive loan; `cell_swap` requires two distinct exclusively borrowed places. |
| `std.container.Local` | `local_empty` creates a caller-owned `Local<T>` slot; `local_peek` snapshots its `Option<T>`; `local_put` installs a value and returns the previous option; `local_take` returns the previous option and leaves `None`. Independent slots do not share state. |
| `std.container.Quarantine` | `quarantine_stage` returns `own<Quarantined<T>>`; `quarantine_check` checks a policy without consuming the staged owner; `quarantine_review` returns `Ok(T)` on acceptance or `Err(own<Quarantined<T>>)` with the same owner on rejection. There is no replacement allocation on rejection. |
| `std.container.Rc` | `rc_sexpr_new` wraps an immutable graph; `rc_sexpr_clone` and ordinary record copies retain it without copying nodes; `rc_sexpr_get` returns a separately retained value. The existing runtime releases the last reference. |

```on1x
import std.container.Box as Box;
import std.container.Local as Local;

fn main() {
    let answer = Box.basis_box_new(41);
    {
        let edit = borrow mut answer;
        let before = Box.basis_box_replace(edit, 42);
        assert(before == 41);
    }
    var slot: Local.Local<i64> = Local.basis_local_empty();
    assert(option_is_none(Local.basis_local_put(slot, Box.basis_box_into_inner(move(answer)))));
    assert(option_unwrap_or(Local.basis_local_take(slot), 0) == 42);
    assert(option_is_none(Local.basis_local_take(slot)));
}
```

`borrow` and `ref` are lexical loan modes, not first-class pointers. Use
ordinary `drop(owner)` for explicit disposal or `move(owner)` to transfer a
named owner after any loan ends. `Cell` is a copyable value for admitted
payloads, not a shared identity: copies get independent storage. For
alias-aware interior mutability, use the separate [checked RefCell](SHARED.md#dynamic-local-refcellt-guards).
`Local` is an
explicit state argument or local binding, not TLS or task-local storage.
`Local.Option` and `Quarantine.Result` are re-exports of the same nominal types
that the automatic prelude exposes; they also work without that prelude.

## Policies

`BasisQuarantinePolicy<T>` dispatches statically on the policy type and the
payload type. There is one built-in implementation: `BasisQuarantineI64Range`
for `i64`, with inclusive `low` and `high` bounds. A reversed range rejects
every value. Other policies can use a locally defined nominal policy type:

```on1x
import std.container.Quarantine as Q;

record RequireFlag { expected: bool }

impl Q.BasisQuarantinePolicy<bool> for RequireFlag {
    fn basis_quarantine_accepts(policy: RequireFlag, value: bool) -> bool {
        return policy["expected"] == value;
    }
}

fn main() {
    let pending = Q.basis_quarantine_stage(false);
    match Q.basis_quarantine_review(move(pending), record<RequireFlag>{expected:true}) with {
        Ok(flag) => assert(flag),
        Err(retained) => drop(retained)
    };
}
```

The `cliche` checks that the policy implementation exists with the correct
signature. It does not prove the policy's logic, purity, or trustworthiness.
Each check or review calls it once; a separate check followed by a review
calls it twice. `Quarantined<T>` is a transparent record, so this workflow does
not prevent deliberate access to a staged value. It is not a security
boundary, sandbox, or runtime-enforced proof that data was validated.

## Payload and failure limits

The helpers work with copy-safe payloads admitted in nominal aggregates, such
as numbers, booleans, fixed-size byte arrays, and `sexpr`. Copying a managed
`sexpr` retains its existing immutable shared reference. Ordinary `string`,
dynamic `buffer`, and foreign handles are not adopted as managed payloads.
`RcSExpr` uses the existing single-threaded immutable S-expression reference
count and preserves its copyable API. See [checked shared owners](SHARED.md)
for the separate move-only `Rc<T>` and `Arc<T>` APIs.
The compiler can hold a move-only payload in `Box<T>` if constructed directly
with `box(record<Box<T>> { value: move(payload) })`: extract it by consuming
`take(owner)` and matching the returned record with `record{value: item}`.
The copy-based generic helpers do not accept such payloads. For nested boxes,
`basis_box_exchange_box(owner, move(replacement))` provides a checked exchange;
the returned owner must be consumed or left to lexical cleanup. This does not
extend the foreign ABI or concurrency model.

Rejection by a policy is a recoverable `Err`. Allocation failure in `box`
instead runs the compiler/runtime's emergency cleanup and exits with status
72; it is not converted into a policy result. Other terminal traps do not
gain general unwinding. See
[checked borrows](../../docs/core-references.md),
[unique owners](../../docs/recursive-owning-adts.md), and
[core cliches](../../docs/core-cliche.md) for the backend limits.

## Tests

From the MuLIB root:

```sh
python3 on1x/tests/test_container_local.py PATH/TO/on1xc on1x /tmp/opencode/container-local-gcc gcc
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  python3 on1x/tests/test_container_local.py PATH/TO/on1xc on1x /tmp/opencode/container-local-san clang \
  --cflag=-fsanitize=address,undefined
```

The test compiles the On1x consumer, builds generated C11 with strict
warnings, and runs native lifecycle and per-allocation failure cases. It also
compiles a no-prelude consumer and checks located ownership, loan, and cliche
rejections. Native allocator hooks are test instrumentation only.

Validated on x86-64 Linux against MuLIB base `c2cbbf70537d` and LibStone
`3399047f6c63`: `build/on1x-workstream4-gcc/on1xc` (generated scanner) with
GCC 16.2.1 and `build/on1x-workstream4-clang/on1xc` (fallback scanner) with
Clang 22.1.8, both at `-O2`. All nine allocation faults and all eleven
compile-time rejections passed. The same test passed with generated C and the
runtime under GCC/Clang ASan+UBSan (leak detection enabled, `-no-pie` with GCC),
and with `build/on1x-language-clang-san/on1xc` as the instrumented frontend.
