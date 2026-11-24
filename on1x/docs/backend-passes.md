# Verified scalar MIR passes v1

The opt-in B4 increment adds a bounded, transactional pipeline over
[scalar MIR](backend-mir.md). It is available in every compiler build, including
the independent fallback build. Public API:
`include/on1x/backend/passes.hpp`; implementation: `src/backend/passes.cpp`.

The v2 numeric/product extension retains the same four algorithms. Constant-fold
now has descriptor version 3. The later On1xMETA integration advances
`local-slots` to version 3; the other pass descriptors remain version 2.
Reports select `portable-scalar-v1` for legacy modules and
`portable-numeric-product-v2` for MIR v2 (including a default API target request).
MIR v3 selects `portable-numeric-v3`; its typed numeric descriptors retain
conservative trap effects and are not signed-folded. Existing value/slot/CFG
transformations remain verified before/after and compare typed payloads exactly.
MIR v4 selects `portable-directives-v4`. Portable passes preserve all
directives and their order; target-specific rewrites are selected at the
product boundary described in [the MIR guide](backend-mir.md).
Preserved effect/barrier identities include the numeric descriptor, so changing
`add_u8` to same-signature `sub_u8` cannot pass a claimed preservation check.
The report envelope remains `on1x.mir-passes` version 1. Float arithmetic,
comparisons and conversions are **not folded**; environment/range-check effects
remain roots. Constant float phi propagation compares exact bit payloads, so
opposite signed zeros cannot be merged. Trivial tuple loads/stores can forward,
but no managed/unique resources, foreign aliasing or ownership events are admitted.
CFG cleanup and dead-pure elimination apply only under the existing verified
rules. Novostone publication remains v1-only and rejects v2/v3 before publication.

## Use

```sh
on1x/build/on1xc --emit-ir --optimize-ir on1x/examples/optimize.on1x
on1x/build/on1xc --run-ir --optimize-ir on1x/examples/optimize.on1x
on1x/build/on1xc --dump-passes on1x/examples/optimize.on1x \
  -o /tmp/opencode/optimization.sexpr
python3 on1x/tools/on1x-build.py on1x/examples/optimize.on1x \
  --via-mir --optimize-ir --on1xc on1x/build/on1xc \
  -o /tmp/opencode/optimized-example
/tmp/opencode/optimized-example
```

The example prints `1`, `43`, `42`. `--optimize-ir` applies to MIR emission,
evaluation, CFG output and optional relational analysis/fact output. All these
modes also accept `--ir-input` saved MIR. `--dump-passes` runs the pipeline and
emits its deterministic provenance report. The ordinary `--emit-c` HIR path
rejects `--optimize-ir`; the build helper requires explicit `--via-mir`.

Optimization defaults OFF. The default pipeline repeats `constant-fold`,
`simplify-cfg`, `local-slots`, `dead-pure` until an entire round leaves MIR unchanged, or a budget
is exhausted. It allocates no new entity IDs: retained definitions keep their
identities and source points, and removed IDs leave gaps.

## Pass semantics

### `constant-fold`

- Propagates exact `i64`/`bool` literals through pure comparisons/boolean negation
  and checked integer arithmetic, using a bounded deterministic worklist.
- Folds a nonentry block parameter only when **every incoming argument** has an
  independently known, identical literal value. The parameter ID becomes a
  constant instruction at block entry; all incoming argument vectors are repaired
  simultaneously. Unknown/self-referential inputs are not assumed constant.
- Checked operations fold only when they cannot trap. Overflow, zero division,
  `INT64_MIN / -1`, `INT64_MIN % -1` and minimum-value negation stay as runtime
  operations with their original source locations and effect ordering.
- Descriptor v3 additionally folds signed-i64 `x-x`, `x*0`, `0*x`, and `x%1`
  to zero even when `x` is unknown. These identities cannot trap, including at
  `INT64_MIN`. Operand producers still execute: calls, loads and earlier
  checked arithmetic retain their effects and traps. `x/x`, `x%x`, and
  `x%-1` are excluded, as are typed Numeric/unsigned and IEEE identities.
- Successful arithmetic folds remove their now-proven-nontrapping effect steps.
  Remaining effects retain their order and output-token identities; input tokens
  and the terminator token are repaired in that same transaction.

Reference evaluation and folding share `src/backend/checked.hpp`, which checks
ranges before executing host arithmetic. Emitted C11 uses the independent
C runtime checked operations. Tests additionally use Python's arbitrary-precision
integers as an independent arithmetic oracle.

The [benchmark feedback](../benchmark/OPTIMIZATION_FEEDBACK.md) records the
v2-versus-v3 diagnostic measurement separately from optimization-off/on results.
The targeted workload drops from 28 to 19 MIR instructions and 26,013 to 17,013
reference evaluation steps. These reductions do not imply general application
speedups.

No call evaluation, slot propagation, algebraic reassociation or path-sensitive
phi speculation is performed. For example, a false edge carrying a nonconstant
condition is not itself evidence that the parameter's incoming value is a literal.

### `simplify-cfg`

Converts constant-condition branches to the selected jump and removes blocks
unreachable from entry. It also simplifies a branch when **both destination and
the complete argument vector** are identical. Two edges to the same block with
different phi arguments remain distinct. Source branch/terminator IDs and
locations are retained. Removal of an unreachable effectful block is committed
only with the complete verified CFG change.

This pass does not merge blocks, remove reachable loops, assume recursive calls
terminate, or reorder effects on surviving paths. A constant-true loop still
diverges after its unreachable exit block is removed.

### `local-slots`

Within each block, tracks the last stored value or first loaded value of each
function-local slot. Subsequent loads forward that SSA value. Stores overwritten
later in the same block are removed: intervening loads already forward their
original values, so captured values retain snapshot semantics. The final store to
each slot in each block is retained to preserve successor initialization/state.
Slot declarations and types remain intact.

Calls cannot access caller slots: verified scalar MIR has no slot addresses,
indirect calls, foreign calls or shared storage. Forwarding therefore remains
valid across direct calls, including recursion. Checked arithmetic, assertions,
calls and printing retain their order and identities. A trap is terminal; earlier
local stores are not externally observable on a trapping path. Computing a stored
value can still trap or print even when the store is removed.

Version 3 additionally forwards an unresolved load from the nearest store on
a chain of unique predecessors. Every traversed predecessor dominates its
successor. The nonrecursive backward walk stops at the entry, a multi-predecessor
join, or a repeated block; every visited block and instruction consumes the shared
work budget. No incoming value is assumed at a join or loop header, and store
deletion remains same-block only. This permits a subsequent folding/CFG round to
simplify a canonical Select value after one of its predecessors becomes
unreachable. It does not add general global slot propagation or memory SSA.

After discovery, substitutions cover all uses, including successor instructions,
return/branch operands and parallel edge arguments. This is independent of block
listing order. Alias chains follow dominance and use bounded path compression.
Effect inputs/terminator tokens are repaired after removing loads/stores; retained
effects keep their output-token identities. The transition verifier additionally
checks exact slot declarations and ordered nonmemory barrier identities/opcodes/
targets/types. Operand substitution and effect-token changes invalidate the
complete callsite/effect views.

Example: `examples/slots.on1x` prints `2, 1, 2, 5, 0, 1, 2`, on separate lines:

```sh
on1x/build/on1xc --run-ir --optimize-ir on1x/examples/slots.on1x
```

### `dead-pure`

Traces value dependencies backward from every effectful instruction and every
return/branch value. A needed block parameter depends on **all** of its incoming
edge arguments. This handles cycles without recursive traversal and removes
entire unneeded pure-value/phi cycles in one transaction. Dead nonentry parameters
and their corresponding incoming arguments are removed together.

All calls, loads, stores, assertions, printing and potentially trapping arithmetic
are roots, even when their results are unused. Entry parameters and all function
signatures remain intact. CFG edges, reachable loops, slots and function
declarations are retained. Removing a pure value cycle is distinct from removing
the control-flow loop containing it.

## Pass descriptors and checked preservation

Each descriptor records name, version, required admission, preserved/invalidated
analyses, effect policy, target, budget policy and verifier. All current passes
require `verified-mir` and one of the two target profiles described above.

| Pass | Preserves | May invalidate |
|---|---|---|
| `constant-fold` | Signatures, CFG graph, callsites | Effects, values, liveness |
| `simplify-cfg` | Signatures | CFG, callsites, effects, values, liveness |
| `local-slots` | Signatures, CFG graph | Callsites, effects, values, liveness |
| `dead-pure` | Signatures, CFG graph, callsites, effects | Values, liveness |

Preservation declarations are checked, not merely advisory cache hints. Before
and after each candidate, the MIR verifier checks identities, types, SSA/CFG,
effect chains and definite slot initialization. `verify_transition` additionally
checks the declared preserved views and every retained entity's source location.
The CFG view covers ordered block identities, terminator kinds and destination
lists; parameter definitions/arguments belong to the value/liveness views.
The effects view includes block boundary tokens and complete ordered effect steps.

These checks do **not** prove arbitrary rewrite equivalence. Only the four
implemented built-in transforms can produce a `passes::Result`; there is no
untrusted callback registry or artifact import that can manufacture one. The
public transition checker is useful for invariant tests, not as an admission
certificate for an arbitrary optimizer. Rewrite legality rests on the explicit
rules above, supported by differential tests.

## Transaction and budget contract

```cpp
namespace p = on1x::mir::passes;
p::Options options;
options.work = 100000;
auto result = p::run(verified_mir, options);
// result.module() is always a verified complete module.
// Inspect result.status(), events(), diagnostic(), and changed().
```

The input module is immutable. Each pass edits a separate candidate, verifies
it, and only then replaces the pipeline's last verified module. A malformed
source, failed structural/preservation check or unsupported target throws an
`ON1X-MIR-PASS`/MIR verification diagnostic; no result or partial artifact escapes.

Resource exhaustion discards the entire active candidate and returns the last
verified **prefix**, which may be the original module. Its status is explicitly
`BudgetExhausted`, never a successful fixed point. The CLI emits that valid prefix
and a diagnostic on stderr, then proceeds normally with the requested mode.
Compilation success in this case means a valid fallback artifact, not completed
optimization. Program traps and evaluator exhaustion retain their existing 70/75
exit codes. On a real compilation error, existing output files remain intact.

Default limits:

- 4,194,304 pass work units and 65,536 attempted rewrite actions, shared across
  the entire pipeline. CLI overrides: `--opt-work N`, `--opt-rewrites N`.
- Eight rounds; API permits 1–64 rounds and 1–16 explicitly ordered built-in
  passes. A fixed point requires a complete unchanged round. Reaching the round
  cap after a changing round reports exhaustion conservatively.
- MIR verifier limits apply independently to each admission/transition check.
  Pass work accounts for candidate copying, indexing, operand/edge visits,
  worklists and mutation, not the verifier's separate work counter or wall time.
- Rewrites count folded values/parameters, simplified branches, removed blocks,
  dead instructions, dead parameters, forwarded loads and overwritten stores.
  Failed candidate attempts consume
  budget but do not advance the logical output revision.
- Canonical MIR and provenance output retain the existing S-expression byte/node
  limits. A provenance report embeds both exact input and output MIR; very large
  reports can hit the 4 MiB writer limit even when standalone MIR emission fits.

`--dump-passes` emits `(on1x.mir-passes 1 ...)` with exact source/output MIR,
target, ordered versioned descriptors, options, consumed budgets, overall status,
and per-attempt round, logical input/output revisions, outcome, attempted work/
rewrites and diagnostic location. Revisions are local to this run; full canonical
MIR supplies the source identity. Source paths participate in determinism. No
persistent optimizer cache or source-independent hash identity is implied.

## Novostone publication

With the optional [relational layer](backend-analysis.md), use
`Store::optimize(expected_snapshot, options, import_limits)`. It runs the same
pipeline, then projects and publishes the verified result against that **exact**
CCDB source snapshot. A stale/foreign snapshot rejects before optimization.
Projection/import failure leaves the database head unchanged. No-op results,
including zero-work exhaustion, retain the original revision. Changed verified
prefixes can publish even if the optimization budget was exhausted.

The returned `Optimization` contains both the published snapshot and its pass
result. CCDB invalidates caches from actual changed base facts, including
retraction of calls in removed blocks. `require_current` continues to reject old
queries after a changed publication. The audit reason identifies complete versus
budget-prefix publication; the returned pass report carries detailed provenance.
These transforms derive legality from MIR, not from untrusted fact rows.

## Verification and scope

`on1x.passes_hir` tests fixed points, explicit pass ordering, checked preservation,
all integer trap families, effect roots, cyclic/live/dead phis, parallel edges,
divergence, source identity, budget sweeps and whole-pass rollback.
`on1x.passes` checks strict emitted C, independent wide-integer arithmetic,
deterministic/readable reports, saved-MIR idempotence, CLI fallback/rejections and
build-helper execution. `on1x.mir` compares five execution paths: HIR C11,
ordinary/optimized MIR C11 and ordinary/optimized MIR evaluation, including exact
output traces and trap diagnostics. Relational tests cover optimized publication,
cache retraction, stale-source rejection and rollback after projection failure.
See [measured toolchains and results](../VERIFICATION.md).

Slot tests additionally cover recursive frame isolation, different join inputs,
loop-carried updates, non-topological block ordering, immutable snapshots of
mutated values, terminal traps, barrier preservation and candidate rollback.
Eight seeded mutation programs (360 updates) compare against an independent
Python state-machine oracle through all execution/readback paths.

This implements the scalar B4 subset. General pass plugins, interprocedural
constant evaluation, inlining, alias/ownership/cleanup transforms, Sequent
optimization, Syren diagnostics, register/spill assignment and VM/native lowering
remain open. Optimization can change evaluator step/storage counts; two exhausted
evaluations are not evidence of semantic equivalence. No performance claim or
universal equivalence proof follows from the finite differential corpus.
