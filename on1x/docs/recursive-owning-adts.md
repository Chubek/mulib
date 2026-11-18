# Recursive nominal ADTs with finite unique ownership

On1x admits recursive nominal records and enums when every layout cycle crosses
an explicit `own<Named>` edge. This is a bounded, single-threaded,
direct-checked-HIR-to-C11 profile. It does not replace the existing
[immutable shared `sexpr` profile](owning-adts.md), and it does not make all
managed or foreign resources generally safe.

```on1x
enum List<T> { Nil, Cons(T, own<List<T>>) }

fn empty<T>() -> own<List<T>> {
  return box(variant<List<T>>(Nil, record{}));
}

fn prepend<T>(item:T, tail:own<List<T>>) -> own<List<T>> {
  return box(variant<List<T>>(Cons, (item, move(tail))));
}

fn sum(values:own<List<i64>>) -> i64 {
  let node = take(values);
  return match node with {
    Nil() => 0,
    Cons(item, tail) => item + sum(move(tail))
  };
}
```

`own<T>` is not a nullable shared pointer type. In the admitted form, `T` must
be a known nominal record or enum, including its exact module identity and type
arguments. The checked type retains that identity and is finite even when the
named declaration is recursive. A direct or mutual by-value cycle still
rejects; only `own<Named>` cuts a nominal layout cycle.

## Operations and transfer rules

The ownership operations are fallback intrinsics:

| Form | Checked behavior |
|---|---|
| `own<Named>` | A unique, move-only owner of one named record or enum value. |
| `box(value)` | Evaluates one nominal value, allocates an owner, transfers the complete value into it, and returns `own<Named>`. A named move-only operand requires `move(local)`. |
| `move(local)` | Transfers one named local or parameter whose type is move-only. The source becomes statically moved-from. It is not valid on a temporary or a copyable value. |
| `take(owner)` | Consumes one `own<Named>`, frees that box without destroying the transferred payload, and returns the named value. |
| `drop(value)` | Consumes any move-only value and destroys its active owned/shared fields exactly once. |

`box`, `move`, `take`, and `drop` are not reserved function names. An ordinary
lexical binding or declared function with the same name wins normal name
resolution. Only an otherwise-unbound direct call is interpreted as an
ownership operation; the intrinsics are not first-class values.

A type is move-only transitively when it is `own<Named>` or a supported nominal
value containing one. There is no implicit copy:

- passing, returning, assigning, or embedding a named move-only local requires
  an explicit `move(local)`;
- `take(local)`, `drop(local)`, and a consuming pattern match are themselves
  explicit consumption sites;
- fresh temporaries transfer directly rather than requiring a synthetic local;
- a callee owns each move-only parameter and drops it on an ordinary exit unless
  the parameter was transferred again;
- a returned move-only value is transferred out before callee cleanup.

The canonical value verifier reconstructs whole-value and projected moves from
checked HIR. It rejects implicit copies, second moves, reads after a whole or
partial move, forged transfer metadata, and ownership state that disagrees at a
control-flow join. Assigning a new value to mutable storage reinitializes it;
the previous value is dropped before overwrite. A repeatable loop cannot leave
an enclosing unique local newly moved on its backedge.

## Construction, matching, and destruction

Operands still evaluate exactly once, left-to-right. Aggregate construction
uses zero-initialized temporary storage and transfers each completed field into
that storage. If a later owning allocation fails, already completed fields and
other live generated values are released by the bounded emergency cleanup
path.

Pattern matching a move-only enum consumes the scrutinee. Its arms must be
exhaustive, unguarded constructor arms; nested tuple/record destructuring must
be closed and irrefutable. Nested refutable constructors, `or`/`as`/literal/
array patterns, guards, and the legacy payload-lambda `match` spelling reject.
This restriction makes the active payload transfer explicit and leaves no
partially live scrutinee after the decision.

Normal lexical cleanup covers:

- expression temporaries and unconsumed parameters;
- block, loop, branch, early-return, and function exits;
- mutable overwrite and reinitialization;
- partially constructed nominal values; and
- only the active arm of a tagged sum.

Named records/enums may combine unique fields with immutable shared `sexpr`
fields. Shared values keep their existing retain/release semantics; moving a
containing value transfers its unique fields without retaining or copying them.
Generated sum destructors switch on the checked tag and never release inactive
union storage.

## C11 runtime profile

Generated C represents an owner as opaque `on1x_unique *` and requires
`ON1X_UNIQUE_ABI_VERSION == 1`. Payload storage has `max_align_t` alignment.
Successful box and take operations clear their source storage so generated
cleanup cannot observe a second owner.

Recursive destruction is iterative. `on1x_unique_drop` appends boxes to a FIFO
work queue; destructors can enqueue child owners without recursively consuming
the C stack. The executable tests release chains of 50,001 boxes, while the C++
ABI test independently releases a 20,000-box chain.

The runtime bounds one payload at 64 MiB and the process-local emergency registry
at 262,144 cleanup records by default. Compiler cleanup expansion and canonical
ownership-flow analysis also have finite work budgets and distinct limit
diagnostics. These limits are resource budgets, not timeouts.

Owning allocation failure is terminal. Before exiting with status 72,
`on1x_unique_allocation_fail` drains the bounded registry of live generated
values, including callers and unrelated shared fields. It is not a recoverable
exception. Existing assertion, arithmetic, bounds, and other terminal traps do
not acquire general unwinding semantics.

The runtime ABI is internal to generated code. Its raw allocation, take,
cleanup-registration, and ABI-version checks fail closed for malformed C/C++
callers.

## Deliberately closed boundaries

This profile does not admit unique ownership through:

- scalar MIR, MIR serialization/evaluation/passes, or other backends; MIR needs
  typed move, drop, place, and cleanup events first;
- Unit-v1 PPX transport, `--emit-ast`, or source units transformed by a PPX;
- raw foreign parameters/results or inferred foreign aggregate layouts;
- closure captures, deferred arguments, staged code, or cooperative
  concurrency;
- Chisel or Nerotypical adapters, which reject rather than approximate it; or
- arrays/slices and unnamed data aggregates. Unique storage is admitted through
  named record/enum layouts and their checked payload structure only.

There are no general borrows of owned values, field mutation, shared unique
graphs, user-defined destructors, recoverable allocation failure, thread-safe
`Send`/`Sync` model, or separate-compilation ownership ABI in this slice.

Inherent implementations continue to reject both `ref self` and `move self`.
Sound consuming receivers require receiver-place and method-call lifetime
lowering outside this increment. Resource-owning values must therefore use
ordinary functions or non-receiver associated functions with explicitly
consuming parameters.

## Tests

From the MuLIB root, with a configured compiler:

```sh
python3 on1x/tests/test_recursive_owning_adts.py \
  PATH/TO/on1xc on1x /tmp/opencode/recursive-owning-adts gcc
ctest --test-dir PATH/TO/BUILD --output-on-failure \
  -R '^on1x\.recursive_owning_adts(_hir)?$'
```

`tests/test_recursive_owning_adts.py` covers generic recursion, modules,
left-to-right evaluation, calls/returns, overwrite and control-exit cleanup,
active sum arms, shared-plus-unique fields, fault injection, strict C11, ABI
drift, and the unsupported boundaries above. The C++ test mutates checked HIR,
checks MIR rejection and the C/C++ runtime ABI, and verifies iterative
destruction. Exact compiler/scanner/sanitizer results are recorded in
[`VERIFICATION.md`](../VERIFICATION.md).
