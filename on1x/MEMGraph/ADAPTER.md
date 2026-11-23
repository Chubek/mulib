# Compiler-side adapter contract

## Executable profile: `c11_fixed_array_v1`

The direct C11 emitter calls the adapter after the canonical control, value,
ownership/loan, nominal, kernel and concurrency verifiers. The SDK's built-in
C11 backend uses the same emitter. This profile observes only emitted
fixed-array index/slice sites; all other normalized operators are outside its
projection. The existing emitter and runtime remain authoritative for them.

When enabled, the [Firestone and Novostone HIR passes](../hir/README.md) run
earlier in the processor. This adapter sees the final rechecked HIR, not
their internal representations or proof records. A folded literal must pass MEMGraph's
ordinary shape, layout and range checks.

`CheckedArrayIndex(base, index)` and `CheckedArraySlice(base, lo, hi)` project
the exact checked operation, result type and source point. Each capture refers
to an original, immutable HIR child in evaluation order; the adapter does not
reconstruct a variable name, lexical place, or foreign reference from rule
text. Fixed shape, exact `i64` operands, extent, and unmanaged layout come
from the checked HIR. Slice values, moves, loan nodes and regex captures are
not eligible.

For a matching `ProvedArrayIndex` or `ProvedArraySlice` replacement, the only
permitted change is to omit a call to the bounds helper. The proof compares
against the original lowering:

| Domain | Evidence |
| --- | --- |
| Value and exact type/module identity | The original expression, operand nodes, result type and access/layout code remain unchanged. |
| Evaluation and effects | C11 emits the same operand evaluation and temporaries in order. The removed helper would return without doing anything. |
| Ownership and loans | No move, release, borrow, allocation or operand evaluation is removed or introduced. |
| FFI identity | No foreign call, ABI adapter or admission metadata is matched or modified. |
| Traps/errors | The compiler shares the C11 runtime's exact in-range conditions; every other check, including those in operands, is still emitted. |
| Location/provenance | Source HIR is not mutated. Retained calls use the original locations; reported decisions also cite the authored rule. |

The proof accepts canonical decimal `i64` literals and small, closed
expressions using checked unary negation and `+`, `-`, `*`, `/`, `%`. It shares
overflow and divisor checks with the C11 runtime. The proof does not replace
or short-circuit those expressions in emitted code. It declines if any
operand is dynamic, if evaluation could trap, or if the expression would
exceed the local budget (at most 15 expression levels and 64 visited
expression nodes across a site's bounds). For an index the predicate is
`0 <= index < extent`. For a slice it is `0 <= lo <= hi <= extent`; the empty
slice at the end is admitted. Unknown
predicates, a declined check, mismatched captures, and work/node/depth budget
exhaustion retain the reference check at that site. After exhaustion, later
sites also keep their checks; earlier independently proved sites remain valid.
The generated `preserves` names are
validated when compiling the package; they are not proof results.

There is no rewritten HIR to revalidate: the candidate is a bounded private
lowering decision, and both outputs use the same verified HIR and C11 path.
Inspect with `on1xc --dump-memgraph`; its bounded report includes applied and
retained checks and whether the budget was exhausted or the events truncated.
It is not an admission artifact or an application-runtime log.

## Contract for future profiles that change HIR

MEMGraph is intended to run on **checked and specialized** On1x canonical HIR,
not source text. Core entry points to inspect in MuLIB are:

- `src/sema.cpp`, `src/infer.cpp`: authoritatively checked type identity;
- `src/canonical/verify_values.cpp`: independently reconstruct ownership,
  initialization, projected moves, borrowed places and lexical loan state;
- `src/processor.cpp`: existing On1xMETA candidate transformation and recheck;
- `src/codegen.cpp`: direct-C11 reference emitter;
- `src/backend/mir.cpp`: current scalar/product verifier (not full ownership);
- `src/safety.cpp`, `include/on1x/safety.hpp`: Chisel-backed admitted extern profile;
- `on1x/contracts-exoml/` and `docs/contracts.md`: claim-backed FFI evidence.

The normalized `MEMGraph` node names in the `.mmgi` files are deliberately
separate from `Expr::Kind`. A host adapter must define a versioned, verified,
lossless projection for each admitted node and reject any unrepresentable HIR.
For example, a `Move` node carries lexical **binding identity** and a checked
source location, not just the spelling of a variable. `ForeignCall` carries
exact target/header/revision/ABI/admission *references*, never self-reported
`safe=true` input from an `.mmgi`. `BoundsValidated` has a frozen extent/
provenance snapshot and checked trap semantics. `ReadOnlyView` has the lexical
loan root and its scope. No proof-bearing metadata may be manufactured by a
rewrite template.

## Required admission procedure for one candidate

1. Run the canonical On1x parser, inference, semantic checks, ownership and
   borrow checks on the **original** HIR. Reject untrusted/source-forged tags.
2. Map only supported checked nodes to the immutable normalized graph. Keep
   effect, evaluation, error, provenance, type, source and ownership identities.
3. Check match shape, capture uniqueness and same order before considering a
   candidate. Graph matched nodes are not mutable author-controlled objects.
4. Evaluate each `requires` obligation using a **registered host predicate**
   over independent checked information; absence/timeout/error is failure.
   No predicate name can invoke user code or load a foreign symbol.
5. Compute the candidate in temporary storage with separate work/node/depth
   budgets. Never mutate the original in place or expose partial output.
6. Re-run canonical source/candidate validators and verify all nine `preserves`
   domains *against the original*: value, exact type/module identity,
   evaluation trace and order, effects, ownership, loans, FFI identity,
   traps/errors, and location/provenance. This comparison may require
   conservative refusal for constructs with effects or unproven equivalence.
7. For FFI, require independently selected pinned review authority, canonical
   CDecl identity, ExoML and Chisel/ABI/effect evidence, exact compatible
   target and runnable differential gate where the admitted profile demands it.
   Never permit a `ForeignCall` rule to turn raw, pointer-bearing or unreviewed
   calls into approved safe imports. Review state is external input to the
   compiler, not editable rewrite output.
8. Publish the candidate atomically only if every gate passed. Otherwise keep
   the original HIR and emit a located failure/declined-optimization record.
9. When lowering to MIR, **reject** ownership, foreign aggregate, borrow,
   callback, cleanup or suspension profiles not represented by the MIR ABI;
   preserving the direct C11 reference route is required.

## Remaining integration milestones

- Add a lossless versioned normalized graph projection with tests for each node;
- implement a host predicate registry whose evidence is compiler-generated;
- compare effect/trap/loan/owner traces, including error and cleanup edges;
- add a transactional candidate builder with bounded work and source locations;
- differential-test C11 output before/after, including allocation failure,
  duplicate-drop, stale-loan, NUL, alias, lifetime and target-ABI negatives;
- add reproducible `ctest` tests on GCC and Clang/ASan/UBSan;
- *then* permit opted-in candidate rewrites by default, if evidence supports it.

The provided `tools/mmgc.py` cannot replace these steps. It compiles rule
syntax and contracts; its manifest and rule tables are not an authorization
or type certificate.
