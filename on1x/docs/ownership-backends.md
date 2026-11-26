# Ownership across the compiler and runtime

The reference semantics are the checked HIR-to-C11 route. The [canonical
value verifier](../src/canonical/verify_values.cpp) remains responsible for
ownership, initialization, aliasing and lexical lifetime. It resolves a
borrow to a binding identity, not simply to its source spelling.

## MEMGraph lexical slots

[`plan_lexical_slots`](../hir/MEMGraph/lexical_slots.hpp) obtains a private
projection from the canonical verifier. The plan identifies the exact HIR
root declaration, optional parent loan, mode, referent type, reads, stores
and checked `ref_replace` sites. It is tied to the verified `Unit` and
must be rebuilt for any transformed tree, including a VM closure-converted
one. The optional
LLSynth-authored bounds rules have no authority over this plan, and turning
them off does not turn off the ownership checks.

The pinned LibStone checkout provides LLSynth for these authored rules.
Chisel's current place/token CFG lacks reborrow ancestry and on its own
cannot validate the On1x projection. Wetstone's alias/ownership analysis is
advisory. See [the upstream API audit](../hir/MEMGraph/SOURCES.md).

## Backend execution

This slice admits `borrow`/`borrow mut`, nested reborrows, reads, `:=`,
and scalar `ref_replace` for `bool` and MIR numeric types. For example,
[`scoped_loans.on1x`](../examples/scoped_loans.on1x) can run with either
`on1xc --run-ir` or `on1xc --run-vm`.

Each admitted alias resolves to an existing typed root slot, including
when a root was an immutable `let` or by-value parameter. Acquisition has
no runtime operation. The root slot is initialized exactly like any other
MIR slot. A read is a snapshot, and a replacement evaluates its RHS before
the old-value load and store; an early return or trap does neither.
Debug and release profiles use the same canonical ownership check.
MIR text, CFG, evaluator, MIR-C11, VMWeave, and Archon consume existing
slot operations and effects; there is no pointer layout or MIR wire change.
Direct C11 and Cephyr retain their checked-C11 route.

No new MIR resource representation is implied by resolving these lexical
aliases before MIR. Pattern-bound roots, managed/aggregate roots, and
parameter borrowing still reject in MIR. General unique/RC values
still require a verified runtime representation and effect ordering for
allocation, drop/retain/release, error edges and function-defer stacks.
Borrowed parameters require call-scoped places and callee effects; they
cannot be modeled as a scalar argument followed by unconditional copy-back.

The separate [nominal-owner MIR experiment](mir-nominal-owner.md) is not a
source route: only its named C++ evaluator and C11 emitter admit its
single-block in-memory form. It does not add owner borrowing, implicit
cleanup, or a transport/ABI to the routes above.

```sh
cmake --build build/on1x-baseline
ctest --test-dir build/on1x-baseline --output-on-failure \
  -R '^on1x\.(ownership_backends|mir_borrow_hir|memgraph\.lexical_slots)$'
```

On Linux AMD64 the same test runs real Archon and Cephyr artifacts when
those products are configured. It also exercises ordinary and optimized
serialized MIR/VM, compares effect order and the first failing operation,
and checks that malformed loans or unsupported layouts do not publish an
artifact.

## Compiler SDK and On1xRT

The compiler SDK exposes lexical borrow modes in source AST and checked-HIR
inspection. The referent type is unchanged. An SDK HIR wire snapshot is
inspection data, not canonical admission evidence. New borrow node tags and
the shared-mode flag require a reader that understands them; older readers
may reject such payloads rather than silently dropping the mode.

On1xRT ABI 1 can invoke a scalar export of generated C11 using owners and
loans privately. Its test covers recursive boxes, borrowed payloads, both
arms of an owning `Result`, deferred retention and release of `sexpr`, LIFO
cleanup and zero remaining allocations. The host sees only scalars, never
guest addresses or handles to these On1x ownership values. This is not an
ownership-transfer ABI, and the module still has native process authority.

Run this fixture with:

```sh
cmake --build build/on1x-baseline --target on1x_rt_embed_ownership
ctest --test-dir build/on1x-baseline -R '^on1x\.runtime\.embedding_ownership$' \
  --output-on-failure
```
