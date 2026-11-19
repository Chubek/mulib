# BACKEND.md — Complete the On1x Backend with LibStone

> **Location:** `mulib/on1x/BACKEND.md`  
> **Audience:** autonomous implementation agents working on On1x.  
> **Mission:** implement real, verified and extensible On1x backends, using the *current* LibStone toolchain without regressing the already-working C11 backend. The minimum production goal remains the Shylock price-prediction application described in `COMPLETION.md`; native code generation is an additional outcome, **not** permission to defer safe MuLIB binding.
>
> **Status of this file:** implementation specification, **not** a statement that the integrations already exist. Prefer verified source and passing tests over design claims.

## 0. Source baseline, revisions and the exact newly added work

**SDK dispatch:** the [built-in backend adapters](backends/README.md) give
the existing direct C11 path and four MIR output modes distinct SDK
registrations and static archives. The canonical emitters/evaluator are
unchanged. A build-host Yellowstone Lua manifest can generate the
registration references; the target C++ linker still links `on1xc`.
This does not add a machine-code backend or change generated program ABIs.

**CSP ABI v5 continuation:** [typed directional channels](docs/concurrency.md)
use runtime-owned descriptor/endpoint records, exact quotas and generic
fixed-size pointer-free byte copies through the existing MuLIB FIFO/rendezvous
and atomic selection. Blocking/try transfer, last-sender close and
heterogeneous poll remain HIR→C11 effects. The earlier
[cooperative cancellation and monotonic deadlines](docs/cancellation-deadlines.md)
use generated function-return cleanup edges and private control offers through
the same selection kernel.
`taskgroup within` expiry drains cleanup and terminates the run. Consuming
cancel waits for running/blocked fibers to drain descendants and defers. These
are HIR→C11 semantics, not typed timeout propagation, suspension/effect MIR or
stackless async. The earlier absence statements below are historical.

**G4 cooperative fiber/channel slice:** `taskgroup`, internal i64 fibers,
typed quota-bearing uni channels and bounded heterogeneous atomic `poll`
execute through the canonical HIR→C11 path and the
existing MuLIB stackful fiber/channel kernels (see [contract](docs/concurrency.md)).
The scalar MIR verifier rejects this profile; it has no effectful ownership/
suspension descriptors. This is neither `async/await` nor a native-thread/VM
backend. A limited POSIX `xfiber` direct-exec path and consuming process
`cancel` also execute through C11. A newly launched internal fiber can be
consumed by an immediately following `cancel handle;` before dispatch. The
external path also admits compiler-parsed single/triple backtick command words
and `launch capture`, whose affine stdin/stdout/stderr capabilities use bounded
`u8` MuLIB channels and nonblocking POSIX pipes. It does not invoke a host shell;
expansion, assignments, command lists, pipelines and redirection remain closed.
The [stream-poll continuation](docs/stream-poll.md) supplies exact u8
process-stream offers through the same MuLIB kernel/C11 ABI. Running/blocked
internal-fiber cancellation and `poll after` deadlines are now available in
the bounded cancellation profile, but typed recovery remains unsupported.
None of these effects is represented by scalar MIR.

**M2 function defer / typed errors:** [the C11 profile](docs/defer-errors.md)
executes function-owned bounded LIFO registrations and exact nominal Result `?`
returns through existing canonical regions/cleanup. Return types and tag facts
are independently checked. There is no new MIR format or general effect/exception
IR: aggregate results and dynamic defer still reject MIR/SDK requests. Fatal
traps and cancellation do not gain unwinding guarantees.

**Nonrecursive nominal data:** [named records/parameterized enums](docs/nominal-adts.md)
now retain exact module/type-argument identities while reusing bounded C11 value
layouts. The checker and emission ingress reconstruct layouts from declarations,
including phantom arguments. This does not add aggregate MIR, recursive owners,
foreign aggregate ABI, cleanup effects or native machine code.

**Core-syntax G1/G2 continuation:** [statement loops](docs/core-syntax.md) now
execute in HIR→C11 and scalar MIR→C11/reference evaluation. Range canonicalization
and checked lexical targets feed existing MIR branches/jumps, without changing
MIR serialization. This does not add cleanup/effectful/concurrency MIR or native
machine code, and does not complete the source-syntax specification.

Guarded structural matches now canonicalize to checked decision regions,
single-initialization storage, branches and lexical exits before either emitter.
Scalar/numeric tuples execute in MIR v1–v3; sums/records/arrays/slices and managed
values remain C11-only and reject unsupported MIR/SDK profiles. No new MIR
serialization format, runtime dependency or native backend is implied.

The subsequent [value-loop increment](docs/value-loops.md) uses these same
regions, single-initialization slots and checked leave/continue edges. A value
operand is acquired before lexical cleanup, and an impossible normal-exit path
traps instead of reading an uninitialized result. Independent verification now
tracks possible as well as definite initialization and rejects repeatable
initialization on loop backedges (including condition expressions). No new MIR
opcode/version, exception unwinding, suspension or foreign ABI is introduced.

**M1 language continuation:** the restricted [`f64` profile](docs/numerics.md)
now includes decimal literals and MIR v2 execution/round-trip/C11 for floats and
trivial tuples. A second M1 slice adds the [bounded value data model](docs/data-model.md):
structural records, closed sums with exhaustive `match`, fixed arrays and owning
snapshot slices as by-value C11 with bounds-checked access; scalar MIR and the
independent adapters reject these kinds instead of approximating them. Existing
scalar source/artifacts retain v1. Float arithmetic
folding, managed aggregate/ownership/effect lowering, VM/native backends and
general safe foreign adapters remain unsupported; the reviewed scalar-only
four-gate profiles (signed-integer v1, bool/binary64 v2, unsigned/binary32 v3) are the sole
safe-admitted exception. Foreign calls still reject in scalar MIR; these
adapters execute through the reference HIR-to-C11 path. Unsigned/f32 scalar
values now have a typed numeric MIR v3, independent evaluation and C11 emission,
and Nerotypical numeric HIR obligations. Resource/closure/data-model MIR is still
incomplete. v1 relational mapping
rejects v2.

Read these sources **before changing code**:

- `README.md`, `VERIFICATION.md`, `docs/architecture.md`, `docs/roadmap.md`, `docs/contracts.md`, `COMPLETION.md`.
- `src/{parser,infer,sema,type,codegen,primitives,staging}.cpp`, `include/on1x/`, `runtime/`, `tests/`, `grammar/{On1x.asdl,On1x.g}` and `tools/{on1x-bind.cpp,on1x-build.py,on1x-contracts.py}`.
- `on1x/third_party/libstone/` (or the checkout selected by `ON1X_LIBSTONE_ROOT`) and *only its real public APIs*, CMake targets, manuals, and tests.
- LibStone `moonstone/nerotypical`, `novostone`, `flintstone`, `firestone`, `blackstone`, `limestone`, `sandstone`, `wetstone` and Moonstone itself.

### Verified snapshot identification (2026-11-14)

- The older On1x `VERIFICATION.md` reports its tested LibStone checkout at `aafc6994e9322c9a46ac8f99854c00a8a37a3d5a`.
- GitHub comparison from that commit to `91293264a531603dfd2319e64b3c1aa40a609783` reports **2 commits**, **50 changed files**, all in `moonstone/nerotypical` and `novostone`.
- At inspection, **MuLIB's `on1x/third_party/libstone` Git submodule pointer and upstream LibStone `master` were both `91293264a531603dfd2319e64b3c1aa40a609783`**. Thus these features are *new relative to the earlier verification baseline*, not necessarily absent from the current pinned checkout.
- New Nerotypical: `moonstone/nerotypical/include/nerotypical/sequent/{Syntax,Verify,Evaluate,Transform,Inspect,Print,Bridge,TypeAdapters,SequentIR}.hpp`, examples, manuals and CMake tests; original Satie-based set/structural typing remains.
- New Novostone improvements: Datalog lexer/parser generated from `Datalog.g` and `Datalog.asdl` using **Aurocks/ASDL**; typed source elaboration; `DatabaseBasedIR/Analysis.dl` integrated into the analysis source; corrections to CCDB transaction import, query validation, speculation verification and CostCC model retirement; new CLI and package/integration tests.
- No evidence from these commits that On1x's present C11 backend has been replaced, that Nerotypical handles general effects, or that Novostone handles On1x's `i64`/`f64`/FFI dialect. They do **not**.

References: [LibStone upstream](https://github.com/Chubek/libstone), [the two-commit comparison](https://github.com/Chubek/libstone/compare/aafc6994e9322c9a46ac8f99854c00a8a37a3d5a...91293264a531603dfd2319e64b3c1aa40a609783), [Nerotypical](https://github.com/Chubek/libstone/tree/master/moonstone/nerotypical), [Novostone](https://github.com/Chubek/libstone/tree/master/novostone).

At every run, re-read pinned SHAs, CMake targets and APIs. Never silently overwrite the user's LibStone submodule, auto-upgrade revisions, or patch vendored LibStone to make On1x tests green. If an upstream change is necessary, deliver it as an explicitly identified upstream patch and test against the real checkout.

## 1. Non-negotiable backend rules

1. **Keep C11 operational.** `on1xc --emit-c` is the semantic/reference backend for the supported language subset. No wholesale rewrite, fabricated parallel compiler, or removal of existing generated/fallback scanners, PPX, macro hygiene, staging or typed HIR.
2. **Preserve On1x semantics, not merely result values.** Exact evaluation order, checked `i64` overflow/division, lexical closure behavior, value-restricted polymorphism, `code<T>` stage separation, source diagnostics, failure/cleanup and foreign effects are observable.
3. **Source syntax remains stable.** Internal IR/ABI/build extensions should not require users to rewrite On1x source. Add compiler flags and internal verification artifacts without changing parsed syntax arbitrarily.
4. **Do not cast across IR semantic gaps.** Novostone's present core is **i32** bit-pattern SSA with wrapping arithmetic, explicit bounded cell memory and **no** calls, floating point, FFI, atomics or native codegen. It cannot faithfully represent arbitrary On1x `i64`, checked overflow, floating point or MuLIB calls by narrowing to i32.
5. **Do not pretend the Sequent IR is general effects IR.** Nerotypical's new Sequent IR implements a *pure*, call-by-need reference semantics, `mu`, cuts, joins and bounded simplification/contification. Existing On1x execution may be strict and effectful. Only a proved-compatible pure subset may use this IR initially.
6. **Always verify transformations.** Type, effect, ownership and CFG invariants before/after passes; differential tests against the C11 reference where meaningful. Novostone finite-input differential validation is a useful test tool, **not universal equivalence proof**.
7. **Make unsupported targets fail closed** with explicit diagnostics. No undefined-opcode emission, unimplemented trampolines, guessed encodings, untracked spills, ABI guesses or implicit fallback labeled “native”.
8. **One implementation translation unit** for selected MuLIB `MULIB_IMPL_*` macros. Native binaries statically link the selected kernels and runtime where permitted. Compiler-only LibStone/ExoML tooling must not leak into an ordinary emitted C11 application.
9. **No C++ exceptions through the C ABI.** Source-location identity, ownership and error distinctions survive every boundary.
10. **Tests are the deliverable.** A generated file without successful execution, negative tests, error handling and provenance is not an implementation.

## 2. Desired backend pipeline

```text
On1x source + imports + PPX/macro expansion + AOT staging
  -> existing checked On1x HIR and elaborated type/ownership information
  -> NEW backend-neutral On1x Core/MIR (identity + locations + effects)
       |-- C11 source emitter (existing backend retained)
       |-- pure, eligible functions -> Nerotypical Sequent IR (optional)
       |      -> sequent verifier -> bounded simplification/contification
       |      -> verified lowering back into On1x Core/MIR
       |-- Novostone Datalog/CCDB facts and queries for supported analyses
       |      -> conservative analysis results, dependency/revision tracking
       |-- Flintstone multi-level dialect lowering (typed, explicit effects)
              -> Firestone IR/selection/scheduling/regalloc as supported
              -> Limestone/Sandstone target assembly/object pipeline
              -> supported native executable, or explicit unsupported error
  + ExoML contracts/checked foreign-call adapters -> MuLIB static libraries
  + optional Prolific profiles -> candidate ranking / safe guarded specialization
```

**Design rule:** the backend-neutral representation is the **source of truth** for runtime semantics and safety obligations, not any single optional LibStone optimizer. A compiler function may choose C11 while another can use native lowering, but the choice must be recorded in the build manifest and not change behavior.

The current C11 emitter is allowed to remain direct from HIR while the new MIR is brought up; establish a differential test before migrating it. Do not build a permanent second semantic frontend.

## 3. Implement the On1x Core/MIR bridge first

Create an explicit, versioned internal representation with verifier, deterministic serialization/dump, source map and stable identities. Place modules under `include/on1x/backend/` and `src/backend/` (names are recommendations; align with real repository structure).

**Minimum entities:** module, function, generic specialization key, block, block argument / phi, typed value, instruction, terminator, literal/constant pool, callsite, closure/env, foreign resource, contract reference, source span, control region, exception/error edge and effect token/ordering edge. Each definition has a stable ID in the compilation unit. Never rely on raw pointers or process-local addresses for cache identity.

**Minimum types:** `i64`, `bool`, `void`, `f64` as added by `COMPLETION.md`, strings/bytes, tuple/record, arrays/slices, callable closures, type applications/specializations, resources/borrows, `Option`/`Result` where implemented and staged `code<T>`. Every type must have explicit target-layout/ABI rules or be rejected for native lowering.

**Semantics:** strict left-to-right evaluation for effectful calls, short-circuit boolean operations, checked integer arithmetic, explicit loads/stores/allocations, explicit drop/cleanup, values/errors on success/failure edges, foreign call effects, lifetime regions and closure environment invariants. Preserve the current C11 foreign/raw extern distinction. Do not embed compiler-only ExoML data as runtime pointers.

**Required compiler operations:** `verify`, human-readable dump, graph/CFG dump, deterministic round-trip, source-to-MIR and MIR-to-C11 for a growing verified subset, structural comparison, and a reference MIR evaluator or instrumentation against the C11 backend. Reject unknown operations or mismatched types, invalid CFG/phi uses, missing return, invalid effect ordering and scope/borrow escapes.

**Tests:** exact `i64` boundaries, effect ordering, nested closures, tuple specialization, error cleanup, array bounds, `f64` nonfinite classification, foreign-error branch, stage leaks, malformed MIR, parser/generator parity and deterministic hashes.

## 4. Nerotypical's new Sequent IR: implement a **sound opt-in** pass

Read `moonstone/nerotypical/README.md`, all three `manual/*.md`, and `sequent/{Syntax,Verify,Transform,Evaluate,Bridge,TypeAdapters}.hpp`. Use the real namespace `moonstone::nerotypical::sequent`, `Verifier`, `Evaluator`, `optimize_checked`, and `Nerotypical::Sequent` CMake target where supported. Do not confuse it with the existing Nerotypical/Satie *type-checker demo AST*.

### What to wire now

- Construct a typed Sequent module from **eligible pure On1x functions** only: immutable values, well-scoped arrows/type abstractions, finite constructors/Bool cases, pure closures and joins where semantic evaluation mode agrees.
- Implement term/continuation/command mapping with explicit cut (`<term || continuation>`), local `mu ret`, `let`, `let rec`, and join/jump scopes; represent call-vs-return continuation exactly, not as arbitrary goto.
- Run Sequent verification before and after every optimization. Preserve a source/identity mapping from original On1x terms to Sequent values/joins and back to the validated MIR.
- Only enable β reduction/contification when their evaluation/strictness preconditions hold. The Sequent evaluator is call-by-need; On1x effects and eager evaluation cannot be assumed to match. For nontrivial lambda arguments, either prove observational equivalence or skip transformation.
- Start with a differential corpus of pure expressions and closed functions. Compare On1x C11, MIR interpreter and Sequent reference evaluator on well-typed and bounded cases.

### What NOT to do

- Do not lower I/O, mutable foreign buffers, ExoML calls, price-kernel callbacks or lifecycle-sensitive resources into Sequent's pure intrinsics and call the result safe.
- Do not claim Sequent is a general AOT/VM backend; its `Evaluator` is a bounded reference interpreter, and its `Transform.hpp` lacks production continuation shrinking/cost modeling.
- Do not import the demo `AstBridge` expecting inferred On1x types. On1x must supply a type oracle/typed adapter; the bridge does not invoke Satie itself.
- Do not enable contification of arbitrary recursion, multi-shot control or effectful continuations without a stated proof/validation boundary.

**Implementation gates:** valid pure MIR -> verified Sequent -> optimized Sequent -> verified MIR -> C11 program, with observed same values and traces; syntax/typing failures; optimization budgets; join capture and `mu` shadowing negative cases; no exponential case duplication. Keep pass OFF by default until this gate passes.

## 5. Novostone: exploit its new source-based Datalog integration correctly

Read `novostone/README.md`, `STATUS.md`, `Datalog/{Datalog.hpp,Datalog.g,Datalog.asdl}`, `DatabaseBasedIR/{Analysis.dl,DatabaseBasedIR.hpp}`, `CCDB/CCDB.hpp`, `QueryBasedIR/QueryBasedIR.hpp`, `SpeculativeIR/SpeculativeIR.hpp`, `CostCC/CostCC.hpp`.

Current supported building blocks:

| Component | Use in On1x | Caveat |
|---|---|---|
| `Novostone::Datalog` | Typed integer/symbol facts, indexed recursive queries, stratified negation, work budgets | Function-free Datalog; no arithmetic aggregates or magic general type solver |
| `Novostone::CCDB` | Revisioned facts, immutable snapshots, transactional writes, invalidation and audit | Never mix facts/results across revisions without dependency validation |
| `Novostone::DatabaseBasedIR` | i32 SSA reference/dominance/liveness and example relational analysis | Existing opcode semantics are incompatible with general On1x values/effects |
| `Novostone::QueryBasedIR` | Safe rewrite workflow, SCCP/DCE/CFG, differential test harness | Its own optimizer rewrites apply to its supported i32 dialect only |
| `Novostone::SpeculativeIR` | Guarded parameter specialization and recovery-frame reference | Only narrow supported guard/recovery subset; no arbitrary JIT OSR |
| `Novostone::CostCC` | Context-specific compiler cost ranking, model versioning/drift fallback | Compiler latency cost, **not** Shylock's economic price predictor |

### Integrate via facts, not lossy values

For the first milestone, store On1x MIR **metadata and relationships** in typed relations: block-edge, value-definition/use, callsite, ownership-region, effect ordering, contract dependency and pass provenance. Use stable integer IDs and symbol strings. Type/ABI information not representable in current Novostone fact types stays in canonical On1x HIR/MIR and may be identified by canonical symbols/hashes. Do not truncate `i64` values or reinterpret `f64` as Novostone `i32` operations.

Define a versioned mapping `on1x -> facts` and a deterministic relation catalog, with checks for missing/extra facts, invalid reference identities, unbounded programs and query exhaustion. Reuse Novostone's parser/runtime for `.dl` rules and avoid reimplementing a second rule-language parser. Use transactional incremental updates per function pass, and publish results only against the exact revision used for analysis.

Suggested useful **declarative analyses**: reachable blocks, transitive call graph, definitions/uses, pure-call candidates, affected functions under contract changes, safe drop-points (preliminary *facts* only), and pass invalidation dependencies. Chisel/Wetstone remains authoritative for proof-relevant safety analysis; Datalog derived facts are not borrow proofs without a sound validated transfer model.

Run the source parser and query integrations against an external build and its installed package; ensure `novostone/examples/reachability.dl` and `novostone/DatabaseBasedIR/Analysis.dl` survive packaging. Treat query `UNKNOWN`/budget exhaustion separately from true absence of a fact.

For optional i32-only micro-benchmarks, allow round-trip through Novostone's native i32 SSA dialect with an explicit verifier proving arithmetic width, overflow and effects agree. Do not promote that bridge to full On1x support.

## 6. Flintstone / Firestone / Blackstone / Limestone / Sandstone as native infrastructure

Use *existing* LibStone components before writing alternatives:

- **Flintstone:** multi-level parametric dialects (`arith`, `cf`, `func`, `mem`, `math`, `vector`, `tuple`, `ptr`, `atomic`, `vm`), validated dialect/target adapters, VM/VM-stack path, and narrow native x86-64/AArch64/RISC-V64 examples. Inspect `flintstone/manual/` and active CMake targets; do not assume broad language coverage from the shipped tiny native targets.
- **Firestone:** graph/SSA/CFG, CPS, types, rewriting/equality saturation, VM, instruction selection (`libisel`), scheduling (`libisched`), register allocation (`libregalloc`), object/bignum/memory facilities. Import exact public classes and test individual target linking.
- **Blackstone:** use the real rewrite/pass APIs for an optimizer pass where suitable; don't invent generic “Blackstone optimize” entry points.
- **Limestone:** reuse implemented scheduling, register allocation, VMWeave and machine-description facilities *after* consulting current APIs and verifying target compatibility.
- **Sandstone:** use inspected target descriptions, encoding/object emission/assembly support; **never** create machine bytes by guessing ISA encodings.
- **Wetstone:** reuse abstract interpretation, dominance/dataflow, alias/effect and symbolic infrastructure as explicit analyses, with sound adapters and declared unsupported cases.

### Target implementation order

1. **Portable VM or Flintstone VM** for the verified scalar/closure subset, with explicit resource and foreign-call interfaces; retain C11 as the reference.
2. **Tiny native scalar functions** (no FFI, GC or complex aggregates) using genuine selection/scheduling/allocation/encoding paths; validate target object via the real assembler/toolchain and execute where architecture permits.
3. **Native calls and ABI**: platform ABI classification, registers/stack, call-clobber, alignment, varargs rejection, typed return, unwind/error/cleanup conventions; verify generated calls with C11 fixtures and independent compiler/oracle.
4. **Owned values, closures and aggregates**, cross-module linking and runtime support; then optional SIMD/vector and target optimizations.

Never treat code emitted for a different target as verified. Record target triple, ABI, features, compiler, assembler, selected passes, fallback and linked MuLIB module fingerprints. Native output must not depend on ExoML's interpreter or a C++ callback trampoline unless explicitly declared and justified by its ABI.

## 7. Analysis and optimization pass manager

Introduce a pass API with `requires`, `preserves`, `invalidates`, `effects`, `target`, `budget`, `version` and `verify` metadata. Passes should be transactional: produce a candidate, run verifiers/analyses, commit only after success; on failed verification leave the source unchanged and emit a Syren diagnostic. Pass results are keyed by module identity, target, contract identities, options and source/IR revision.

Mandatory passes: lowering/canonicalization, CFG normalization, checked constant folding, dead pure code elimination, liveness, foreign effect barriers, closure conversion, verified register/spill assignment for supported native targets, and exact-once cleanup placement. Additional passes (inlining, Sequent simplification, equality saturation, loop transforms, vectorization and speculative specialization) begin disabled until soundness gates pass.

**Profiler feedback:** Prolific measurements and Novostone CostCC may *rank* compiler strategies; they may not waive type/ownership checks, claim semantic equivalence or certify a risky rewrite. Profile-guided speculation requires a guard, a sufficient fallback, correct state reconstruction, invalidation and real workload comparison. Price prediction in Shylock is distinct from compiler profitability prediction.

## 8. Foreign calls are backend features, not ad hoc source rewrites

Use the canonical ExoML CDecl plus v2 semantic contract and the independent On1x admission gates in `COMPLETION.md` / `MOONSTONE.md`. Backends consume a **typed, verified foreign-call descriptor** with:

- exact C symbol, full signature/layout/target ABI, calling convention, const/restrict/alignment, parameter count and enum domain;
- owner/borrow/transfer/return lifetime, destructors, output validity on failure, effects and thread/suspension constraints;
- checked conversion for `size_t`, C enums, `double`, arrays and aggregates; borrowed input extent and writable output capacity;
- edge-by-edge error and cleanup semantics, not an unexplained `int -> bool` cast;
- link plan: transitive `MANIFEST.yaml` modules, one `MULIB_IMPL_*` unit, startup/runtime dependencies and `-lm`/platform flags as needed.

The backend must preserve these requirements in C11, VM and native lowering. `extern fn` raw escape hatches stay explicitly unsafe. Safe imports only appear when the full source review + ABI + compiler usage + runtime gates succeed. Otherwise emit a deterministic rejection or name the adapter **trusted/unchecked** rather than certifying safety.

Required Shylock fixtures: `mulib_statsprice_estimate`, `mulib_histprice_ema`, `mulib_price_forecast`, `mulib_price_ensemble_blend`, `mulib_price_demand_fit`, and `mulib_priceopt_optimize`, including failures, arrays, structs, enums and checked result output. Use the *actual* canonical header signatures.

## 9. Build integration and proposed CLI

- Pin/display LibStone SHA and `ON1X_LIBSTONE_ROOT` in the build report. Prefer checked-out CMake packages/subdirectories or explicit imported targets; do not reach into private source/build directories of other projects.
- Include optional features independently: `ON1X_WITH_NEROTYPICAL`, `ON1X_WITH_NOVOSTONE`, `ON1X_WITH_FLINTSTONE`, `ON1X_WITH_NATIVE_BACKEND` (these are **proposed option names**, not claims of current support).
- For Nerotypical the standalone public target is `Nerotypical::Sequent`, but its new `CMakeLists.txt` is *not currently registered by `moonstone/CMakeLists.txt`*. Wire it without duplicating targets and add an integration test.
- For Novostone the public targets include `Novostone::Datalog`, `Novostone::CCDB`, `Novostone::DatabaseBasedIR`, `Novostone::QueryBasedIR`, `Novostone::SpeculativeIR`, `Novostone::CostCC`. Confirm they resolve when installed and relocated.
- Add backend selection flags only when implemented: proposed `on1xc --backend=c11|vm|native --target=... --emit-ir ... --dump-passes ...`. Preserve `--emit-c` and all prior flags. An unimplemented backend must exit with a clear failure.
- Tooling should write generated artifacts into the build directory; no generated parser/IR C source committed merely to mask a missing generator.

**Useful upstream smoke commands** (from a LibStone root, with real available dependencies):

```sh
cmake -S novostone -B /tmp/libstone-novo -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/libstone-novo --parallel
ctest --test-dir /tmp/libstone-novo --output-on-failure
/tmp/libstone-novo/novostone datalog novostone/examples/reachability.dl Reach

cmake -S moonstone/nerotypical -B /tmp/libstone-sequent -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/libstone-sequent --parallel
ctest --test-dir /tmp/libstone-sequent --output-on-failure
```

These commands exercise upstream components, **not** an On1x backend. Add independently runnable On1x end-to-end tests.

## 10. Required test matrix

| Test | Mandatory evidence |
|---|---|
| Existing On1x regression | GCC and Clang generated/fallback scanner tests, PPX/macros, JSON, staging, closures and C11 output |
| Backend verification | malformed CFG/phi, use-before-def, ABI mismatch, effect reorder, type/region escape, missing cleanup and invalid target diagnostics |
| Sequent adapter | pure differential C11/evaluator tests, join scope, `mu`, β sharing, tail-only contification and budget exhaustions |
| Novostone adapters | source grammar parse, typed facts, exact revisions, stale/cache rejection, Datalog budget exhaustion, no i32 truncation |
| VM/native | correct arithmetic including checked overflow, stack/ABI and call evaluation order, closure lifetime, target feature mismatch |
| FFI | real MuLIB kernels/arrays/structs/errors, ABI layout probes, ownership rollback and one implementation TU |
| Safety | ASan/UBSan; TSan for eligible concurrent components; negative compile tests and allocation-failure injection |
| Reproducibility | deterministic IR dumps/artifacts, pinned source/contract/toolchain IDs; independently built installed-package consumer |
| Shylock | real On1x source ingesting a fixture, generating forecasts, comparing MAE and safely handling no-demand-data case |

If a toolchain/target cannot be run, say **NOT VERIFIED** and provide the exact blocker; do not present compilation as execution or a sample corpus as a proof.

## 11. Implementation work packages and acceptance gates

- **B0 — Audit and pin:** authoritative baseline map, current submodule pointer, build/CTest status and actual CMake exports. *Pass:* no invented APIs; reproducible dependency report.
- **B1 — MIR and verifier:** checked, deterministic intermediate layer with source identity, type/effect/ownership edges. *Pass:* round-trip, malformed negative tests, C11 differential baseline.
- **B2 — Nerotypical bridge:** restricted pure lowering and verified return, bounded optimization. *Pass:* readback semantic equivalence on a rigorous targeted corpus and explicit rejection of effects.
- **B3 — Novostone relational analyses:** generated Datalog rules, transactional fact import and revisioned queries. *Pass:* analysis equivalence against an independent On1x reference oracle; no i32 type laundering.
- **B4 — General backend pass manager:** preserved/invalidated facts, verified pass order, diagnostic provenance and deterministic fallback. *Pass:* failed transformations never escape into executable artifacts.
- **B5 — Real VM/native subset:** choose actual Flintstone/Firestone/etc. target APIs and produce executable artifacts. *Pass:* GCC/Clang C11 oracle agreement; real machine execution on supported host and ABI fixture checks.
- **B6 — Shylock-ready static foreign calls:** `f64`, buffer/record/enum/error/resource bindings through ExoML-reviewed contracts. *Pass:* `COMPLETION.md` Shylock runnable and safe-FFI gates (which are separate).
- **B7 — Additional optimization / profiling:** measured profile-driven optimization and guarded speculation, optional async/vector/native improvements. *Pass:* verified deoptimization, strict fallback, no observable semantic regressions.

B6 is the user's actual application requirement. **Do not hold B6 hostage to B5 or B7**. C11 plus verified foreign bindings is an acceptable Shylock-ready implementation.

## 12. Deliverables, change reports and completion criteria

For every work package commit: (1) source/headers/CMake updates; (2) positive and negative tests; (3) public docs and examples; (4) a reproducible command log; (5) an explicit supported/unsupported feature table. Update `README.md`, `VERIFICATION.md`, `docs/roadmap.md` and `COMPLETION.md` only with measured facts.

**Backend complete for Shylock** only when an On1x-authored application builds with real MuLIB kernels, emits a standalone C11 executable and passes the `COMPLETION.md` functionality/ABI/error/ownership tests. **Native backend complete** additionally requires verified machine code for each explicitly supported ABI and language subset. **Optimizing backend complete** additionally requires validated pass transformations with bounded failure behavior, sustained performance evidence, and safe fallbacks.

Do not close the task with placeholders, a prose-only roadmap, hardcoded demo outputs, unverified speculative transforms, an `emit-safe` command that bypasses review, or a fake native target that compiles by calling the old C emitter. Continue implementing against the real repository, and report blockers with evidence rather than inventing success.
