# Compiled semantic contracts

`hir/On1xMETA/semantic.o1m` is compiled by the LLSynth-generated On1xMETA
frontend into the same immutable table used by the canonical processor.
`hir/On1xMETA/processor_semantic.cpp` interprets its constraints directly over the
actual On1x HIR. It does not invoke another checker as a rule action.

## Admission and ordering

The existing type, ownership, borrow, control, concurrency and extension
checkers remain authoritative. `src/sema.cpp` handles admission and hands
the checked HIR to `hir/optimize.cpp`:

1. Existing macro/stage/inference preparation and elaboration checks.
2. Compiled structural semantic contracts.
3. Existing control lowering, type checking and advisory analysis.
4. Existing value lowering/rechecking, concurrency lowering and mandatory
   control/value/ownership/concurrency/extension verification.
5. Compiled typed semantic contracts on the original, verified HIR.
6. A private candidate: run optional Firestone and Novostone, then canonicalize
   so that a neutral operand such as
   `!false` is visible before `&&` expands. Then desugar bottom-up, trying
   canonical rules before desugar rules at each node and after each
   replacement. For example, in `x && (true != false)`, normalize the new
   negated equality to `true` before visiting `&&`. This avoids building a
   decision region that a later expression-only pass could no longer remove.
   Optional partial evaluation runs between the two authored stages.
   Both rule families share the second traversal's finite work/rewrite budget.
   Check typed contracts, types and all mandatory canonical invariants after
   the changing prepass and before publication.
7. Commit the candidate and its report only after verification succeeds.
   The `canonicalize` report sums visits and per-rule hits from its traversals.

An invalid operand in a constant/dead branch must fail the original checks
before rewriting can erase it. An analysis diagnostic escalated to an error
also prevents processor publication. No macro expansion, inference or source
lowering is repeated on the private canonical candidate. The ordinary checker
accepts canonical CSP poll offers when validating this candidate.

PPX input remains untyped source syntax with its existing schema and permission
boundary. Source stages still undergo their established AOT lowering.
Compiler-side `.o1m` contracts neither execute PPX programs nor grant foreign
admission. All ordinary `check` callers, including interface checking and
source-to-MIR/SDK routes, use this ordering.

## Executable contract vocabulary

Each constraint selects an expression kind, or a `Stmt`-prefixed statement
kind, and carries an authored name, message and rule-file source point.

| Property | Check |
|---|---|
| `arity`, `min_arity` | Exact/minimum number of expression operands |
| `callee`, `body` | Required/forbidden callee or statement body |
| `value` | Required/forbidden statement value; `boolean` additionally requires bool after checking |
| `type` | Exact declared bool/i64/void result annotation |
| `resolved` | Variable has a checked reference identity |
| `operands` | Operand types are equal, boolean or numeric |
| `result` | Result annotation equals operand zero |
| `forbidden` | Reject a selected constructor |

The shipped contracts protect unary/binary/call shapes, scalar/variable/unit
leaf shapes, tuple arity, source match/arm shape, value-block tails,
projections, bounds, propagation, borrows and staged code operands. Typed
contracts check booleans/unit, resolved references, binary operand agreement,
unary result agreement and control-condition types. They inspect annotations
without inferring or repairing them.

This is a bounded set of structural and typed invariants, not a complete
language semantics, proof of rewrite equivalence, replacement ownership
analysis or independent foreign-ABI certification. Retained generic prototypes
and universal proof bodies remain under their existing authoritative checks;
the typed contract walker does not treat these as executable monomorphic HIR.
The structural walker still checks their supported shapes.

## Budgets, diagnostics and provenance

`processor::validate(const Unit&, SemanticStage, const Limits&)` is read-only.
Traversal counts functions, parameters, patterns, expressions and statements.
Every visited node and selected constraint consumes work. Child traversal is
deterministic: callee, operands, structural proof bodies, patterns/parameters,
then statement bodies; statements visit their value before nested branches.
Null operands/instances and invalid source locations reject explicitly.

Effective budgets are the minimum of the caller's limits and the compiled
program budgets: currently 262,144 nodes, 8,388,608 work steps and depth 520.
Exhaustion reports `ON1X-PROCESSOR-LIMIT`. Contract violations report
`ON1X-PROCESSOR-SEMANTIC` at the original HIR source point, with the authored
rule name and `.o1m` file/line/column in the message. They are distinct from
the ordinary source type/ownership diagnostics.

`include/on1x/sema.hpp` adds:

```cpp
void check(Unit&, analysis::Context&, processor::Report&);
```

The report is reset on entry and marked verified only upon successful commit.
`processor::report` returns deterministic JSON with structural/typed visit and
constraint counts, desugaring/canonicalization rewrite counts, and rule-source
provenance, plus compiled program/parser SHA-256 identities. No process-global
report or change to `Unit`'s transport ABI is
needed. The CLI exposes this as `--dump-processor`.

## Focused verification

`tests/test_processor_semantic.cpp` exercises malformed pre-lowering match
shapes, hidden expression bodies, null operands, missing conditions,
inconsistent typed operands/results, forged boolean annotations, unresolved
checked identities, read-only failure, all three budget boundaries, source
rule execution, deterministic provenance and invalid-source preservation.
The source/backend differential suite is `tests/test_processor.py`.

### Focused worker verification, 2026-11-21

Baseline MuLIB `94b27511328c61a219c90cf531ca68e952bc2a85`, configured LibStone
`5d733d69f19fac1d8a05294fe54e4a4ec863fc4c`; Linux x86-64, GCC 16.2.1,
Clang 22.1.8. The isolated fallback-scanner build used SDK dispatch and genuine
LLSynth rule generation:

```sh
B=/mnt/andromeda/tempdir/opencode/on1x-meta-worker3
cmake -S on1x -B "$B" -DBUILD_TESTING=ON \
  -DON1X_BUILD_NATIVE_PPX=OFF -DON1X_USE_FALLBACK_SCANNER=ON \
  -DON1X_BUILD_EXTENSION_SDK=ON -DON1X_WITH_WETSTONE=OFF \
  -DON1X_WITH_SYREN=OFF
cmake --build "$B" --target on1xc on1x_processor_semantic_tests --parallel 4
ctest --test-dir "$B" -R '^on1x.processor_semantic$' --output-on-failure
ctest --test-dir "$B" \
  -R '^on1x\.(processor_integration|poly|staging|ppx|concurrency|core_ref|defer_errors|value_loops)$' \
  --output-on-failure --parallel 3
```

The focused unit passed **1/1** and source regressions passed **8/8**.
A separate short-circuit/effect-order source probe produced identical output
through strict GCC/Clang builtin-SDK HIR-C11 and MIR-C11 execution (four runs)
and real Archon AMD64 native assembly execution. The Archon tool/ABI archive
were from the existing `on1x-archon-build`; the source-to-MIR compiler was
the newly built worker compiler.

The semantic validator and its unit were separately compiled with Clang
`-Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined` and passed.
The linked frontend/scanner archives in this focused sanitizer run were
uninstrumented; it is not full-compiler sanitizer coverage. Optional Chisel,
Nerotypical and additional platforms were not exercised by this worker.
These checks do not establish whole-language completeness.
