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
| `result?` | For the exact nominal `Ok(T)`/`Err(E)` protocol containing a unique owner, consumes the complete result, transfers only the active payload, and either continues with `T` or returns the same nominal result and `E`. |

`box`, `move`, `take`, and `drop` are not reserved function names. An ordinary
lexical binding or declared function with the same name wins normal name
resolution. Only an otherwise-unbound direct call is interpreted as an
ownership operation; the intrinsics are not first-class values.

A type is move-only transitively when it is `own<Named>` or a supported nominal
value containing one. There is no implicit copy:

- passing, returning, assigning, or embedding a named move-only local requires
  an explicit `move(local)`;
- `take(local)`, `drop(local)`, move-only `result?`, and a consuming pattern
  match are themselves explicit consumption sites;
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

These are static representation properties, not user-implementable traits:

| Checked shape | `Copy` | explicit/consuming `Move` | lexical `Drop` | `Send` / `Sync` |
|---|---:|---:|---:|---:|
| Scalar without managed fields | yes | not required | no | outside this profile |
| `own<Named>` | no | yes | yes | no |
| Nominal record/enum containing an owner | no | yes | yes, active fields only | no |
| `Result<T,E>` containing an owner | no | yes, including postfix `?` | yes, active variant only | no |

There is no derive or protocol override for these properties. A type is
move-only transitively, and source cannot declare a unique value copyable.
This profile is single-threaded and callback-free: it neither proves thread
affinity/reentrancy nor admits owners as `Send` or `Sync`.

`own<Named>` owns a compiler-created box. It is not a wrapper around an
arbitrary `i64`, pointer, descriptor, socket, terminal handle, or foreign
allocation. Existing generation-tagged scalar adapters remain raw bridges;
they are not promoted to this safe type. A checked external owner still needs a
reviewed constructor/drop/error/effect ABI, invalidation rules, and affinity
contract.

## Recoverable construction and `Result`

The nominal two-variant `Result<T,E>` protocol from
[typed error propagation](defer-errors.md) now composes with this finite owner
profile. Both success owners and owned errors are admitted:

```on1x
enum Result<T,E> { Ok(T), Err(E) }
enum Node { Node(i64) }

fn node(value:i64)->own<Node> {
  return box(variant<Node>(Node,value));
}

fn acquire(fail:bool)->Result<own<Node>,u8> {
  let first=node(1);
  if fail {
    // first is still owned here and is dropped on this Err return.
    return variant<Result<own<Node>,u8>>(Err,7u8);
  }
  return variant<Result<own<Node>,u8>>(Ok,move(first));
}

fn use(fail:bool)->Result<i64,u8> {
  let pending=acquire(fail);
  let owner=pending?; // consumes pending
  let value=take(owner);
  return match value with {
    Node(number)=>variant<Result<i64,u8>>(Ok,number)
  };
}
```

Postfix `?` is an explicit consuming operation for a move-only result, like a
consuming match. A named operand is marked moved as a whole; the selected
payload is moved into either the continuation slot or the returned `Err`; and
the generated source slots are cleared. Reading the result again, removing a
transfer marker in checked HIR, or copying either payload rejects. Temporaries
transfer directly.

A constructor may therefore return a normal `Err` after partially acquiring
owners. Reached function defers run first, then unreturned local owners drop in
reverse lexical order. An owner placed in the returned `Ok` or `Err` is saved
before those cleanups and is not dropped by the callee. This is ordinary typed
failure, not exception unwinding.

Owning allocation failure remains terminal. `box` exhaustion drains the
bounded emergency registry and exits 72; it is not converted into `Err`, and
the allocation budget is not a timeout.

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

Postfix propagation uses the same rule for a nominal result containing an owner.
The complete result operand is consumed, and the selected `Ok` or `Err` payload
is a checked projected move. The source and transferred payload storage are
cleared, so active-arm cleanup cannot observe a duplicate owner. The enclosing
function must return the same nominal result declaration and exact error type;
the success type may differ. Nested propagation, generic owner targets and
module-qualified identity retain the ordinary typed-`Result` rules.

Non-owning deferred arguments may coexist with this propagation. A managed
return value is registered with the emergency cleanup registry while function
defers and trailing lexical drops run, then disarmed immediately before the C
return. Thus an owning allocation failure inside a reached defer also drains the
pending `Ok`/`Err` return. Capturing an owner or saving one as a deferred
argument remains rejected.

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

`on1x_unique_take` also clears the internal caller slot. A second take through
that cleared slot fails closed. This tests stale transfer state without
dereferencing freed storage. The raw opaque pointer ABI is internal; arbitrary
foreign pointer reuse or a raw double-drop is not a supported checked operation.
Generated On1x code prevents those cases statically and by clearing moved
storage.

Recursive destruction is iterative. `on1x_unique_drop` appends boxes to a FIFO
work queue; destructors can enqueue child owners without recursively consuming
the C stack. The executable tests both consume and destroy independent chains
of 50,001 boxes without recursive On1x or C calls, while the C++ ABI test
independently releases a 20,000-box chain.

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
- closure captures, owner-bearing deferred arguments/results, staged code, or
  cooperative concurrency. Copy-only defers may coexist with owner locals and
  run on a propagated `Err`;
- Chisel or Nerotypical adapters, which reject rather than approximate it; or
- arrays/slices and unnamed data aggregates. Unique storage is admitted through
  named record/enum layouts and their checked payload structure only.

The later [typed Result continuation](defer-errors.md) can move the active
`own<Named>` success or error payload through `?` and clear that source arm
before cleanup. It does not open the PPX, foreign, capture, deferred-argument,
staging or concurrency boundaries above.

The separate [checked borrow profile](core-references.md) can lend an
`own<Named>` place as its `Named` payload for a lexical block or direct call.
The owner cannot move, take, or drop while that loan is live, and move-only
payload fields cannot be copied or moved through the alias. This does not add
borrowed fields/elements, field mutation, shared unique graphs, user-defined
destructors, recoverable allocation failure, a thread-safe `Send`/`Sync` model,
or a separate-compilation ownership ABI.

There are no general first-class borrows, checked scalar/foreign owners, or
ownership-constrained generic helpers. Generic pervasive Result helpers remain
copy-safe; concrete owner-bearing `Result` construction, matching and `?` are
the admitted path.

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
active sum arms, shared-plus-unique fields, owner-bearing `Result` success/error/
nested/defer paths, fault injection, strict C11, deep iterative consumption, ABI
drift, and the unsupported boundaries above. The C++ test mutates whole and
projected transfer metadata, checks MIR rejection and the C/C++ runtime ABI, and
verifies iterative destruction. Exact compiler/scanner/sanitizer results are
recorded in [`VERIFICATION.md`](../VERIFICATION.md).
Additional fixtures cover partial acquisition, defer/cleanup order, double
propagation, stale take state, strict C11/C++17 boundaries, and owned-error
Results.
