# Checked shared and exclusive borrows

On1x has a direct-C11 borrow profile for named lexical places:

```on1x
record Counter { value:i64 }

fn read(counter: borrow Counter) -> i64 {
    return counter["value"];
}

fn increment(counter: ref Counter) {
    counter := record<Counter>{value:counter["value"] + 1};
}

fn main() {
    let owner = box(record<Counter>{value:4});

    {
        let view = borrow owner;       # shared lexical borrow
        println(read(view));           # 4
    }
    {
        let edit = borrow mut owner;   # exclusive lexical borrow
        increment(edit);
    }
    println(read(owner));              # 5
    drop(owner);
}
```

`borrow T` and `ref T` are parameter **modes**, not reference-valued types.
`borrow T` takes a shared call-scoped loan and `ref T` takes an exclusive one.
Likewise, `let view = borrow place;` and `let edit = borrow mut place;` create
shared and exclusive aliases until the end of their lexical block. A bare alias
in a value expression reads the referent; `:=` replaces the value through an
exclusive alias. Ordinary `=` still follows the binding's normal mutability and
cannot write through a borrowed alias.

`ref_replace(place, next)` is a fallback intrinsic for exchanging a complete
value through a named exclusive alias or `ref` parameter. It starts an
exclusive reborrow before evaluating `next` once. The replacement cannot
access that place (or an overlapping loan); compute a snapshot first if
needed. If evaluation returns early (for example, via `?`), the original
value remains installed. On success the replacement takes its place and the
old value becomes an ordinary expression result, subject to normal cleanup.
For move-only values, this transfers the old value without copying it; a
named replacement requires `move(next)`. No box allocation is needed for
the exchange itself.

The exchange does not introduce an uninitialized borrowed slot or a movable
borrow. It cannot be saved in `defer`, used as a function value, or applied
to a field/index expression, shared alias, or unborrowed variable. A declared
function or lexical binding named `ref_replace` takes precedence.

The source must be one initialized, addressable named local, parameter, or
incoming borrowed parameter. Literals, call results, fields, indices, closure
captures, globals, and temporary expressions are not loan origins. Binding
identity follows lexical scope rather than spelling, so shadowing is handled
without conflating places.

## Loans, reborrows, and values

Multiple shared loans may coexist within the finite compiler limits. An
exclusive loan excludes every other access to the same root. A shared loan
excludes writes, moves, drops, and exclusive loans but permits reads and further
shared loans. Reborrowing an exclusive alias temporarily suspends that alias;
nested shared and exclusive reborrows are supported, including sequential
chains in one block, and the parent becomes usable again when the child scope or
call ends. An exclusive loan cannot be derived from a shared alias.

Loans are lexical for `let` aliases and call-scoped for borrowed parameters.
Branch-local loans end before a join, and an iteration-local loan ends before
the loop backedge. The checker composes this loan state with the canonical
whole-value and partial-move state. Moving, taking, dropping, or partially
moving an owner/referent while a loan is live rejects, as does borrowing an
already moved or partially moved place.

Borrow aliases are transparent places rather than first-class reference
values. For a copy-safe referent, `let copy = view;`, a normal by-value call, or
`return view;` takes an ordinary value snapshot. This does not extend the loan.
A move-only referent cannot be copied this way, and a borrowed place can never
be moved or consumed. Closures cannot capture a borrowed alias or parameter;
borrowed calls cannot be retained by `defer`; and functions with borrowed
parameters cannot be used as callbacks or other first-class function values.

Borrowing an `own<Named>` place exposes its `Named` payload, not the owning
handle. Therefore the declaration is `fn inspect(x: borrow Named)`, not
`borrow own<Named>`. The owner remains responsible for cleanup and cannot be
moved, taken, or dropped until every payload loan has ended. Generic borrowed
functions are supported after the existing whole-unit specialization step.
Imported and aliased module functions remain direct private calls.

## Calls and evaluation order

The callee must be a direct named On1x function. Arguments are checked in
left-to-right source order. A call loan starts when its argument is reached and
remains active while later arguments are evaluated. Consequently a later
argument cannot conflict with an earlier shared or exclusive loan. Distinct
roots can be borrowed together, and a by-value read before a later loan is an
ordinary snapshot.

The callee has no route by which to retain the caller's place. Borrowed
parameter modes are forbidden on `main`, lambdas, extern declarations,
interfaces, cliche/inherent members, and generated extension functions. Raw
pointer values are not introduced into the language.

## Lowering and fail-closed boundaries

Canonical HIR verification independently reconstructs binding identities,
loan ancestry, move/drop state, branch/loop scopes, and call argument order.
Malformed modes, origins, referent types, stores, captures, calls, and escaped
borrow expressions reject with located diagnostics. Borrow analysis has a
separate finite work budget; exhaustion is `ON1X-BORROW-LIMIT`, never successful
admission.

The C11 emitter re-runs canonical verification. Shared aliases/parameters lower
to `const T *`; exclusive ones lower to `T *`. No copy-back, heap allocation,
`restrict`, or public pointer ABI is introduced. For `own<Named>`, generated
code obtains the payload through the size-checked `on1x_unique_borrow` runtime
accessor. The static checker supplies temporal validity; the accessor also
fails closed for null/taken handles and size drift, but is not a raw-C temporal
safety mechanism.

This profile is deliberately synchronous and direct-C11:

- any unit that combines lexical or parameter borrows with cooperative
  task/suspension constructs rejects;
- scalar MIR, MIR serialization/evaluation/passes, PPX Unit v1/`--emit-ast`,
  `.on1xi`, Chisel, Nerotypical, and structural adapters reject rather than
  erase or approximate loan metadata; advisory Wetstone translation declines
  functions it cannot model and grants no admission;
- public/external SDK providers receive neither checked HIR nor loan
  descriptors and remain closed; the private built-in C11 bridge is the same
  canonical emitter described above, not a separate admission;
- raw FFI, callbacks, closure capture, staged retention, field/index loan
  origins, separate compilation, and cross-thread/coroutine transfer remain
  unsupported.

Terminal runtime failures retain the existing non-unwinding behavior. The
borrow profile proves only the admitted On1x/HIR-to-C11 slice; it is not a
general foreign-effect, pointer-provenance, or concurrent ownership proof.

## Tests

Run from the MuLIB root with a configured compiler:

```sh
python3 on1x/tests/test_core_ref.py \
  PATH/TO/on1xc on1x /tmp/opencode/core-ref-tests gcc
ctest --test-dir PATH/TO/BUILD --output-on-failure \
  -R '^on1x\.core_ref(_hir)?$'
```

`test_core_ref.py` executes shared/exclusive scalar and nominal loans, nested
reborrows, joins and loop backedges, generic/module calls, unique-owner payload
access and cleanup, strict GCC/Clang C11, ASan/UBSan, runtime accessor faults,
budget exhaustion, and located rejection cases. `test_core_ref.cpp` mutates
checked HIR to verify fail-closed C11 ingress and deterministic lowering.
`test_ref_replace.py` covers the exchange through strict generated C11,
including RC parameter lifetime, owner-bearing boxes, typed early return,
located rejection and injected allocation failures.
