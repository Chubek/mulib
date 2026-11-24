# On1x HIR

On1x lowers directly into `on1x::Unit` and its typed expressions and statements.
Parsing and source transforms build the tree; inference, specialization,
canonical control/value lowering, and the On1x verifiers establish *checked
HIR*. There is no second source language or external HIR format. `Unit` is
also used before admission: its C++ type alone does not establish that the
invariants hold.

```text
On1x source -> parse / imports / macros / PPX / staging -> check and canonicalize
   -> checked HIR -> Firestone -> Novostone -> On1xMETA / partial evaluation
      -> final checked HIR (+ MEMGraph lexical slot plan / optional bounds proofs)
         -> lower to verified On1x MIR -> backends/mir-*
         -> direct C11 reference -> C toolchain / optional Cephyr
```

- [`optimize.cpp`](optimize.cpp) owns the checked-HIR rewrite transaction.
  It orders Firestone, Novostone, authored canonicalization, optional partial
  evaluation, and bottom-up desugaring/canonicalization. The semantic checker
  supplies the recheck. Rewrites work on a private clone, and the original
  HIR is replaced only after the candidate passes. Rule selection can
  disable rewrites but cannot disable source admission.
- [`On1xMETA/`](On1xMETA/) owns the On1xMETA grammar, rule tables, semantic
  constraints, and typed partial evaluator.
- [`MEMGraph/`](MEMGraph/) projects lexical copy-scalar loan roots from the
  canonical verifier. Its separate bounds optimization proves redundant
  checks on demand from final verified HIR. Direct C11 and VM-product MIR
  lowering query the bounds proofs in their emission order. Neither plan
  is supplied by PPX, plugins, serialized artifacts, or authored rules.
- [`lower_mir.cpp`](lower_mir.cpp) owns HIR-to-MIR lowering and verifies the
  result before dispatch. The VM path can also recheck a private
  [closure conversion](vm_closures.cpp) before lowering finite products.
  Standard MIR remains numeric/scalar and bounded tuples. Checked scalar
  lexical aliases are resolved to typed slots before MIR; unsupported
  aggregates, owner payloads, parameter loans, cleanup, suspension and
  foreign operations reject rather than silently falling back. Direct C11
  covers the existing richer
  language and remains the default. The [MIR optimizer](../docs/backend-passes.md)
  and [MIR relational analysis](../docs/backend-analysis.md) are separate
  downstream stages.

## Extending the MIR route

Keep source-only syntax out of executable HIR. Add each missing MIR capability
end to end: define explicit typed operations (for example places/loans and
move/drop on each exit edge), extend the MIR verifier and versioned format when
needed, and admit it only for backends that implement those operations. Compare
operand order, traps, cleanup, and diagnostics against direct C11. A structural
verification pass is not a proof that a transformation preserves behavior.
Foreign effects, ABI identity, and suspension/cancellation require their own
admission gates; neither a HIR rewrite nor a successful C compilation can
supply them.

## Firestone

Build with `-DON1X_WITH_FIRESTONE_HIR=ON` (from `ON1X_LIBSTONE_ROOT`) and select
`--enable-opt firestone-hir`, or put `optimization "firestone-hir" on` in the
root `On1xfile`. It is off under the default, debug and release profiles.
`--dump-processor` reports constant folds and divide-by-one rewrites.

Firestone folds full-width signed `i64` `+`, `-`, `*`, `/`, `%` and unary `-`.
Every folded result must also match On1x's independent checked arithmetic.
For example, On1x traps on `INT64_MIN % -1` even though Firestone returns zero.
The divide-by-one rule retains the original numerator, evaluated exactly
once at the same point, and removes only a literal `1` and the nontrapping
divide. It does not simplify `x % 1` or `x / -1` for a dynamic `x`.

The pass visits at most 32,768 HIR nodes and has a bounded traversal depth.
It runs on a private copy after mandatory verification, before Novostone
(if selected) and authored On1xMETA rewrites. The processor rechecks the
transformed HIR before continuing. MEMGraph sees the final rechecked HIR and
still decides independently whether to omit a bounds check. There is no
unconditional memory, FFI, ownership, or float rewrite.
See the [Firestone benchmark](../benchmark/FIRESTONE_HIR.md) for reproduction.

## Novostone

```sh
cmake -S on1x -B build/on1x -DON1X_WITH_NOVOSTONE=ON
cmake --build build/on1x --target on1xc on1x_hir_action_oracle
build/on1x/on1xc --enable-opt novostone-hir --emit-c example.on1x -o example.c
build/on1x/on1xc --enable-opt novostone-hir --dump-processor example.on1x
ctest --test-dir build/on1x -R '^on1x\.novostone_hir$' --output-on-failure
```

`ON1X_NOVOSTONE_PROVIDER=SOURCE` uses the selected `ON1X_LIBSTONE_ROOT`;
`PACKAGE` selects an installed Novostone package. The pass is off under the
default, debug and release profiles. Explicitly enable it by name or with
`--all-optimizations`. Use `--profile debug --enable-opt novostone-hir` to
measure it apart from the authored rules and partial evaluator.
For project builds, put `optimization "novostone-hir" on` in the root
[`On1xfile`](../stdlib/on1xfile/README.md#compiler-optimizations).

## What the pass changes

After mandatory source checks and canonical HIR elaboration, the processor
creates a private clone. The pass can replace a closed `i64` or Boolean
expression with a same-type literal at the original source location. It
supports signed `+`, `-`, `*` and unary `-`, plus `==`, `!=` and Boolean `!`.
On1x independently checks each intermediate with its checked arithmetic and
requires it to fit signed `i32` exactly before admitting the region to
Novostone's wrapping `i32` dialect. A result within range does not excuse an
out-of-range intermediate. Calls, variable reads, division/remainder,
floating point, memory operations, short-circuit expressions and ownership
operations are not projected. Independently eligible children can still fold
without removing or moving their enclosing operation.

The processor rechecks the candidate, applies its normal authored rule stages,
then rechecks and publishes the complete HIR. The normal C11 and MIR consumers
take that HIR. When `memgraph-bounds` is enabled, MEMGraph analyzes it before
direct C11 or VM-product MIR emission. For example, a previously too-deep fixed-array index
can become a literal, but MEMGraph still checks its own shape, exact type,
extent and range before omitting the bounds call. The Novostone pass does not
produce bounds, FFI, ownership or loan certificates.

## Role of each Novostone component

| Component | Use |
| --- | --- |
| Datalog and CCDB | Relational rewrite queries over verified, revisioned IR facts; transactionally publish only the verified local candidate. |
| DatabaseBasedIR | Build, verify and evaluate the bounded single-block SSA projections. |
| SpeculativeIR | Guard and verify compile-time binding of known literals, using its guarded evaluator. No native guards or deoptimization are emitted. |
| QueryBasedIR | Discover rewrites and compare source and candidate on the singleton input domain of a closed expression. |
| CostCC | Rank baseline and optimized projections with the untrained deterministic heuristic. This is not a runtime performance prediction. |
| PCC | Replay the exact candidate rewrites and emit SMT obligations; optionally discharge them through Satie. A reported counterexample prevents the fold. `Unknown` is not proof: even without a solver, a fold requires independent checked On1x evaluation and an exact literal endpoint. |
| ActionSemantics | Synthesize a separate checked-i32 C11 interpreter used as a differential test oracle. It is not linked into `on1xc`'s production pass. |

This integration doesn't emit Novostone native code and doesn't use the
optional signed deployment/package tooling. Neither is an input to the
processor or MEMGraph. Existing C11 emission stays available without any
Novostone or solver dependency in generated applications.

The pass caps an individual projection at 48 SSA values and 24 nesting levels,
and caps a compilation unit at 64 optimization attempts and 32,768 walk visits.
Query and PCC also enforce work/size limits. Inspect `novostone_hir` in
`--dump-processor` for the number of attempts, guarded specializations,
applied folds, obligations and SMT-proved obligations, as well as whether
the pass exhausted its walk, attempt or analysis budget. No report field is
an admission token.
