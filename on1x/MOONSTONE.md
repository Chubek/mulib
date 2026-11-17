# MOONSTONE.md — Integrate the Complete Moonstone Toolkit into On1x

> **Location:** `mulib/on1x/MOONSTONE.md`  
> **Audience:** an autonomous coding agent implementing the compiler, safe MuLIB Static FFI, analysis passes and runtime of On1x.  
> **Purpose:** use the *real* components in `Chubek/libstone/moonstone/` coherently and safely; do not merely include their headers or describe hypothetical future libraries.  
> **Companion instructions:** `COMPLETION.md` is the Shylock readiness contract; `BACKEND.md` governs IR, Novostone and code-generation integration. Read both.  
> **Truthfulness:** this file is an implementation mandate. A proposed adapter, pass, interface or test described below is **not** already shipped unless the current checked-out source and executable tests prove it.

## 0. Inspect actual dependencies and record their revisions

The authoritative source is `https://github.com/Chubek/libstone/tree/master/moonstone`. On1x obtains it through the real checkout at `on1x/third_party/libstone` or the configured `ON1X_LIBSTONE_ROOT`. Check actual paths; some previous docs used `<mulib>/third_party/libstone` instead. Never guess that both exist, quietly patch the vendored checkout, or silently fetch a different revision.

At inspection on **2026-11-14**, the MuLIB submodule pointer and LibStone upstream `master` were both `91293264a531603dfd2319e64b3c1aa40a609783`. On1x's older `VERIFICATION.md` records testing against `aafc6994e9322c9a46ac8f99854c00a8a37a3d5a`: two subsequent upstream commits added Nerotypical's Sequent IR and extended Novostone, which `BACKEND.md` describes. This snapshot identification is **not** a substitute for checking the actual checkout at build time.

Read, in order:

1. `moonstone/{README.md,CMakeLists.txt,docs/INTEGRATION.md,docs/STATUS.md,docs/core-v1-design.md}`.
2. `moonstone/core/{Core.hpp,Result.hpp,SourceManager.hpp,Diagnostics.hpp,SafetyEffects.hpp,moonstone.h}`.
3. `moonstone/exoml/{README.md,STATUS.md,manual/semantic-contracts.md,manual/c11-discovery.md}` and `include/exoml/{CDecl.hpp,Contract.hpp,Extract.hpp,StaticFFI.hpp,AutoBind.hpp}`.
4. `moonstone/chisel/include/chisel/{Chisel.hpp,Regions.hpp,GC.hpp,MemTKXAdapter.hpp}`.
5. `moonstone/syren/include/syren/{Syren.hpp,Repair.hpp,RuntimeDiag.hpp}` and `catalog/`.
6. `moonstone/supersonic/include/supersonic/{Supersonic.hpp,CEGIS.hpp}` and `lua/`.
7. `moonstone/plaid/{README.md,STATUS.yaml,continuum/,weaver/,loom/,conduit/,chorus/,parallax/,sentinel/,ir/,runtime/,integration/,manual/}`.
8. `moonstone/prolific/{README.md,DESIGN.md,STATUS.yaml,SPECIR.md,Speculation.hpp,Prolific.hpp,prolific.h,lua/,tests/}`.
9. `moonstone/nerotypical/{README.md,manual/,include/nerotypical/,tests/}` including the new `sequent/` headers.
10. Related LibStone libraries as needed: `wetstone/`, `novostone/`, `firestone/`, `flintstone/`, `blackstone/`, `limestone/`, and real `third_party` dependencies; canonical MuLIB `../include`, `../manual`, `../MANIFEST.yaml` and `../library`.

Do not infer a capability from a directory name. The latest Moonstone `README.md` still calls the six older subsystems its complete set; **Nerotypical** is a seventh, separately added tree with a standalone CMake target and is not yet integrated into the parent `moonstone/CMakeLists.txt`. Treat that registration/packaging discrepancy as real integration work.

## 1. Architectural boundary: who owns what

```text
                    On1x sources / libraries / applications
                                      |
                     On1x frontend / typed HIR / MIR
                    /           |                 \
      Moonstone safety      Nerotypical         Moonstone utilities
      - ExoML contracts     - Sequent IR        - Syren diagnostics
      - Chisel checking     (pure functions)    - Supersonic synthesis
      - Wetstone analyses                     - Prolific profiling
                    \           |                 /
                     validated On1x MIR + obligations
                                      |
                     C11 compiler / optional native or VM
                                      |
                      MuLIB C11 kernels + On1x runtime
                                      |
                optional Plaid cooperative runtime / services
```

**Source-of-truth split:**

- **MuLIB** owns C declarations and kernel behavior. Its headers, manuals, manifest and tests are authoritative on algorithm semantics, C layout and runtime lifecycle.
- **ExoML** owns compiler-derived CDecl ABI discovery, semantic contract schema v2 and its independent source/ABI provenance. It does **not** automatically establish foreign behavioral safety.
- **On1x** owns language typing, ownership/effect checking, the imported safe API, code generation, user-visible diagnostics and application semantics.
- **Chisel/Wetstone** provide analyses whose proved/implemented scopes must be respected. They do not automatically certify the original C implementation.
- **Moonstone's other components** are specialized services and passes. No source may infer “safe/verified” merely because one service returned success.

Generated ordinary On1x **C11 executables** must not require the ExoML interpreter, libclang, C++ Moonstone libraries or the On1x compiler. Plaid's C runtime, Prolific's recorder and any additional native support are *opt-in* dependencies for applications requesting them, not hidden dependencies of every MuLIB kernel call.

## 2. Moonstone Core: a shared source and diagnostic foundation

Use the real `moonstone::v1` Core facilities as building blocks for compiler tooling, never as excuses to throw away On1x's existing source IDs or error messages:

- `SourceManager`: own UTF-8 source snapshots, half-open **byte** spans and persistent source identity. Preserve On1x offsets, import provenance, macro/PPX expansion contexts and locations across HIR/MIR/foreign wrappers.
- `Result<T>` and structured `ErrorCategory`: distinguish `InvalidInput`, `Unsupported`, `BudgetExhausted`, `NotFound`, `Internal`, with nested context rather than default-success states. This is a **C++ compiler-tool result**; it is not automatically an On1x `Result<T,E>` language value.
- Diagnostics: stable IDs/severity/location/cause, deterministic ordering, suppression and exhaustion accounting. Keep notes and related source spans where known.
- `SafetyEffects.hpp`: preserve foreign effects and unknown effects without downgrading them to pure.
- `moonstone.h`: respect the versioned C ABI and catch exceptions at C++/C boundaries.

**Implementation tasks:** create one canonical span/diagnostic mapping between On1x and Moonstone with explicit byte-column vs UTF-16-column conversion for SARIF. Maintain 1:1 origin identity through generated bindings and passes. Add multi-file/macro/PPX tests, invalid UTF-8, non-BMP characters, concurrent source-manager access, truncation/budget and deterministic JSON/SARIF outputs. Source locations must never be fabricated for generated code; use provenance stacks instead.

## 3. ExoML: complete the Static FFI from evidence to safe On1x imports

### What exists now

The public ExoML SDK already includes a generated `.g`/`.asdl` DSL frontend, in-memory StaticFFI services, checked C++ native binding, compiler-generated C++ wrapper/ABI-probe facilities and opt-in C11 source discovery via libclang. ExoML v2 semantic contracts describe ownership, lifetimes, errors, effects, callbacks, destructors, invalidations and concurrency. Its `Contract.hpp` / `schema.exocontract.json` are canonical.

On1x's `tools/on1x-bind.cpp` implements real extraction, lossless CDecl transport,
pin/check/explain, verification metadata and narrow scalar adapters (signed
and unsigned integers, `_Bool`, binary32 `float`, binary64 `double`, void result). `emit-safe` admits only the
reviewed scalar-only profiles after review metadata, complete profile eligibility,
real Chisel use checking and executable target gates. Integer-only v1 evidence
remains compatible; typed bool/f64 uses v2 and unsigned/f32 uses v3. General ownership/resource-safe
admission remains closed; current `contracts-exoml/` sidecars for JSON, Vec and
Parsec are annotated/unreviewed, not certified. An ExoML Gate B pass or a reviewed
flag does not prove sound client-side use. See [the implemented boundary](docs/contracts.md).

### Implement these gates in order

| Gate | Required evidence | On failure |
|---|---|---|
| A — Discovery | complete extracted CDecl, true target, compiler context, preprocessing/provenance hashes, declaration inventory | reject import; preserve incomplete artifact for diagnosis |
| B — Contract consistency | exact v2 schema, known functions/parameters, checked pointer lengths/lifetimes/transfer and effect metadata | reject contradictory/unmatched contracts |
| C — Human/independent review | reviewed source/manual behavior, revision and review evidence by an actual authorized reviewer | leave `annotated`/`unreviewed`; never self-certify |
| D — ABI | verified scalar width/signedness, `double` representation, aggregate size/alignment/offsets, enum domain, calling convention, C compiler probe | reject unsupported/unknown target or ABI |
| E — On1x usage proof | sound ownership/region/effect checking for *each* callsite, including possible failures and control-flow joins | require explicitly unsafe/trusted adapter or reject |
| F — Adapter runtime | sanitizer and allocation-failure tests, output preservation/failure, exactly-once release, no invalid borrows or callback escape | reject safe emission |
| G — Reproducibility | pinned source/contract/manifest/toolchain/target/dependency closure and stable generated output | invalidate cached admission and rebuild |

Build an eligibility report with per-function reasons and exact source/contract/ABI hashes. **No fake reviewer identity, auto-reviewed metadata, or unconditional `safe_export: true`.** If the review has not happened, the function is not independently reviewed, even if an agent wrote every line and tests pass. Preserve the existing explicit unsafe raw/handwritten adapter mechanism for incremental development.

### Types and calling conventions that On1x must support for MuLIB

- C `float`/`double`, signed/unsigned integers and `size_t` with checked conversions; NaN/inf rejection when a kernel requires finite values; platform-specific width evidence.
- Stable C enum membership, record-by-value and record-pointer layout, constness, alignment and nested structs; reject unsupported union/bitfield/variadic signatures until separately proved.
- Borrowed `const T*` plus count/stride/byte extent, writable output storage, aliasing/disjointness requirements, nullable pointers only where permitted.
- Foreign-owned unique handles, borrowed views and invalidation on mutation, reallocation, owner move/drop; exact destructor family and cleanup under returns, failures and future unwinding.
- C error channels: success flag, error integer, NULL, sentinel, out-error, `errno`; commit output only if valid. Do not read uninitialized `out` after failure.
- Effect tracking for read/write/alloc/free/IO/thread/suspend/unknown. An unknown-effect call is not eligible for pure Sequent optimization, speculative duplication or unsafe suspension.
- Callback descriptors: synchronous vs retained, userdata lifetime, register/unregister, thread affinity, cancellation and exception boundary. Unsupported callback patterns remain unavailable in the safe API.

### Minimum kernel contract coverage for Shylock

Prioritize fully classified ExoML profiles for the actual signatures in:

`mulib-statsproc.h`, `mulib-statsprice.h`, `mulib-histprice.h`, `mulib-competprice.h`, `mulib-priceopt.h`, `mulib-price-forecast.h`, `mulib-price-ensemble.h`, `mulib-price-demand.h`, `mulib-price-confidence.h`, `mulib-price-observation.h`, `mulib-price-history.h`, `mulib-price-timeseries.h`, `mulib-price-normalize.h`, `mulib-money.h` and selected CSV/JSON data kernels.

Prioritize stateless borrowed-array numerical methods first, then owned history/time-series handles. Add concrete negative tests for `mulib_histprice_ema` (borrowed doubles + result record), `mulib_price_ensemble_blend` (two non-aliasing outputs), `mulib_price_demand_fit` (paired arrays + output struct) and `mulib_priceopt_optimize` (demand/config structs + enums). Use actual headers; never manufacture an incompatible C prototype.

**One-translation-unit rule:** gather the transitive `MANIFEST.yaml` dependency/implementation-switch closure and define each `MULIB_IMPL_*` macro in one implementation source. The generated program never defines duplicate implementation switches across consumers.

## 4. Chisel: make foreign ownership and borrow checking sound for admitted operations

Current `chisel/Chisel.hpp` provides a small explicit CFG model with operations such as `Create`, `Use`, `Mutate`, `BorrowShared`, `BorrowUnique`, `EndBorrow`, `Move`, `Drop`, `Reinitialize` and `Escape`; `Regions.hpp`, `GC.hpp` and `MemTKXAdapter.hpp` provide related scaffolding/experiments. It is **not** a complete non-lexical-lifetime or Rust-like ownership checker, and its demonstration GC is not a production collector.

### Required adapters

1. Map **all** On1x HIR/MIR operations into Chisel (including block edges, returned/escaping values, explicit and implicit drops, closure captures, foreign ownership and potential error/unwind exits). Unknown ops must produce a conservative `unknown-effect` barrier/rejection, never disappear.
2. Represent owned resources, shared and exclusive borrows, borrow origins, field projections, alias relationships, slices/extents, return-linked lifetimes and invalidation events. Distinguish a borrow from a copied value object.
3. Extend CFG path sensitivity for `Result` success/failure, partial initialization, conditional ownership transfer, loop fixpoints, returns and future exceptions. Reject double free, use-after-move, borrow escape, mutation while aliased, invalid owner drop and potentially-live reinitialization.
4. Model API-specific invalidation: Vec growth invalidates buffer borrows; JSON replacement can invalidate subtree borrows; price-history insertion may move backing storage; an owning result freed on failure must not be used; borrowed scratch memory cannot escape its validity window.
5. Integrate Wetstone alias/effect/dataflow infrastructure only through **tested sound adapters**. A Wetstone suggestion is not automatically a Chisel proof. Preserve solver `unknown` and exhaustion separately from `valid`.
6. Permit GC only with complete roots, write barriers, object tracing and mutator/safepoint protocols, verified against real MemTKX semantics. Do not switch on the experimental collector as a shortcut to foreign ownership.

**Exit test:** standalone On1x negative fixtures that produce targeted source diagnostics for all listed illegal patterns; real MuLIB allocation/reallocation/failure tests under ASan/UBSan; no invalid borrow survives a foreign mutation in either C11 or alternative backends.

## 5. Syren: consistent semantic diagnostics, actionable repairs and runtime reporting

Syren supplies S-expression diagnostic rules, catalog/ranking/repair interfaces, stderr/SARIF output and runtime diagnostic recording. Use `Syren.hpp`, `Repair.hpp`, `RuntimeDiag.hpp` and `catalog/static`, `catalog/runtime`.

### Implementation

- Map parser/typing/macro/FFI/borrow/ABI/optimizer/concurrency errors into a stable catalog. Include: `ON1X-FFI-ABI`, `ON1X-FFI-CONTRACT-DRIFT`, `ON1X-FFI-LIFETIME`, `ON1X-FFI-UNKNOWN-EFFECT`, `ON1X-OWNERSHIP-USE-AFTER-MOVE`, `ON1X-OPT-INCONCLUSIVE` and `ON1X-PLAID-SUSPENSION-UNSAFE` (proposed IDs, not existing ones).
- Attach real spans and related spans, import stack, contract identity, kernel signature, foreign header/manual provenance and exact suggested fix. Retain deterministic ordering and suppression status.
- Generate structured compiler diagnostics and SARIF; source columns in On1x are UTF-8 byte offsets, while SARIF needs correct Unicode/UTF-16 region conversion as applicable.
- A proposed fix is **never** an accepted fix until independently reparsed, type checked, ownership checked, ABI checked and validated against the original semantics. Keep invalid/unknown/exhausted statuses distinct.
- Permit safe runtime diagnostic collection without trying to unwind arbitrary C foreign frames or swallowing a destructor failure.

**Exit test:** bad foreign signature, stale header hash, invalid enum, borrow invalidation, double drop and insufficient compiler-analysis budget produce correct distinct errors with valid source ranges. SARIF validates with an independent parser/schema checker.

## 6. Supersonic: bounded synthesis, not unchecked optimization

Supersonic currently implements bounded enumerative synthesis of **pure integer expressions**, CEGIS callback support and Lua synthesis grammar integration; it does not offer a complete typed program synthesizer nor a production SAT/e-graph integration out of the box.

Use it for narrowly scoped compiler assistive tasks:

- Candidate generation for equivalent checked-`i64` arithmetic expressions, source repairs or pure constant-expression simplification, **not** f64 algebra under unproved rounding assumptions.
- Exhaustive checks on small finite domains and/or independent counterexample/proof oracles. Do not turn “no counterexample found” or an optional legacy sampling result into a proof.
- Structured output carrying grammar version, source/IR identity, budget, proof status, counterexample and solver assumptions.
- Syren fix proposals can be generated with Supersonic, but a separate On1x compilation and semantic validation pipeline must approve them.

If Satie or Equinox-NG support is enabled, link and test the real vendored APIs, with strict namespaces and no hidden header collision workarounds. Solver UNKNOWN must remain UNKNOWN. Make synthesis entirely optional for basic Shylock compilation.

**Exit test:** overflow counterexamples, unacceptable transformations, budget exhaustion, maliciously large grammars, invalid candidate syntax and a correctly validated small positive rewrite.

## 7. Nerotypical: set/structural typing plus new Sequent IR

This newest Moonstone tree contains two **distinct** facilities:

1. Original `Analysis.hpp`, `SetTyping.hpp`, `StructuralTyping.hpp`, `Language.hpp` using Satie for set and structural type constraint solving.
2. Newly added header-only `sequent/` language: typed terms, continuation frames, commands/cuts, local `mu ret`, joins/jumps, constructor cases, a verifier, reference **call-by-need** evaluator, bounded optimizer and conservative tail-only contification. `AstBridge` and `TypeAdapters` are limited adapters, not an automatic On1x typed-AST conversion.

**Work:** keep On1x's current type checker authoritative until a verified bidirectional mapping exists. Consider Nerotypical's solver as an optional helper for compatible structural/set constraints. Lower **only pure, semantically compatible functions** to Sequent IR; preserve original strict evaluation/observable effects outside this path. Always run `Verifier` and `optimize_checked` and verify the lowered result again in On1x MIR. Guard against `mu` capture, free ambient labels, unbound type variables, continuation duplication and invalid tail-call contification. This is detailed in `BACKEND.md`.

**Build gap:** new standalone `Nerotypical::Sequent` is defined under `moonstone/nerotypical/CMakeLists.txt`, but the parent `moonstone/CMakeLists.txt` does not yet register it. Integrate as an optional feature with an installed-package test rather than declaring it available from `Moonstone::Core` by association.

**Exit test:** pure numerical function through On1x -> Sequent -> optimized Sequent -> C11 matches the canonical On1x reference; impure MuLIB calls and ownership-sensitive operations are explicitly ineligible.

## 8. Plaid: scheduler, channels, actors and eventual language concurrency

Read `plaid/STATUS.yaml`, not merely its README. Plaid **currently implements** one-shot callback continuations, a reference integer CPS machine, synchronous effects, coroutine frame liveness analysis, C++20 manual coroutines, cooperative scheduling, work-stealing pool/task scopes, bounded/rendezvous channels, buffered receive-only select, serial local actors, chunked parallel loops/reductions, scalar lane mapping, limited affine dependence analysis, offline race/lock-order checks and a cooperative **C ABI** runtime.

Plaid does **not** yet implement arbitrary Scheme `call/cc`, multi-shot resource-safe continuations, automatic On1x `async`/`await` lowering, stackful fibers, async I/O reactors, full atomic rendezvous select, native SIMD vectorizer, GPU backend or a fully instrumenting race detector.

### Build usable integration in increasing order

**P1 — explicit runtime embedding:** add an opt-in On1x runtime surface for cooperative tasks and safe typed channels via Plaid's *real* `PlaidRuntime.h`. Ensure C ABI versioning, no C++ exception escape, ownership of task payloads and cleanup when tasks are cancelled/closed. Only expose operations that the runtime truly implements.

**P2 — static effect and suspension checks:** mark blocking, thread-affine and non-suspension-safe ExoML imports. Reject crossing a suspension point with a prohibited live foreign loan. Ensure mutable/non-thread-safe MuLIB history or Vec instances never migrate to worker threads through shared raw pointers; use owned immutable snapshots or explicitly synchronized designs.

**P3 — On1x async/compiler lowering:** use `weaver/AsyncAnalysis.hpp` to compute spill/liveness sets; define an explicit state machine and cancellation/drop paths in On1x MIR, then generate and test actual C11 state machines. `PlaidIR.hpp` currently has an opcode vocabulary and basic use-before-def validation, **not** full compiler lowering. Do not claim an opcode list is an implementation of async/await.

**P4 — data parallel Shylock processing:** permit parallel independent product/model computations only where underlying kernels and buffers are proven thread-safe, without shared mutable histories or allocator lifetimes. Protect deterministic reductions and stable error aggregation. Respect worker shutdown and bounded queues, and make cancellation cleanup total.

**Exit tests:** TSAN-supported stress tests for channel close/send/receive, pool stop, actor destruction, cancellation, task scopes and foreign calls; ASan lifetime tests; explicit serial reference comparison; no inferred thread safety for an unannotated C kernel.

## 9. Prolific: real instrumentation and safe profile-driven decisions

Prolific is a compiled profiling subsystem with a versioned C11 recorder ABI, LMDB cache, JSON reports, optional Linux function-instrumentation injection and vendored Lua analysis tools. It also emits SpecIR Lua speculation *advice* with validated `lspecir` constructs; it does **not** install machine-code guards or prove optimizations.

### Integrate the compiler first

- Instrument On1x frontend phases, ExoML extraction/admission costs, analysis passes, Sequent simplification, Novostone queries, C11 generation, VM/native codegen and actual memory/compile-time usage where observable.
- Use stable site IDs, On1x source/IR revision, target/ABI, optimization pipeline, hash of contract catalog, and build/workload context so profiles for unrelated builds cannot be combined.
- Track rejection/loss/collection incompleteness explicitly: uninstrumented time, missing events and failed recorder admission are not zero cost.
- Use profiler events to advise the pass manager and Novostone CostCC's compiler-cost ranking. Never use cached popularity or measured latency to override type checking, Chisel proofs, contract reviews or ABI verification.
- If guarded specialization is enabled, actually emit a checked guard, fallback/deoptimization path, assumption version, preserved state and invalidation test. SpecIR on its own is only a validated *instruction proposal*.
- Keep ordinary generated On1x applications free of Prolific unless profiling was requested; the toolchain's opt-in LMDB/Lua/Kaguya dependencies must not contaminate default executables.

**Real CMake targets:** `Moonstone::Prolific` and `Moonstone::ProlificLua` when dependencies exist. Current Prolific requires vendored Lua/Kaguya/lmdbxx and the LMDB C library/header. If unavailable, record an explicit **SKIP/NOT AVAILABLE** for profiling features and keep the compiler functional with `MOONSTONE_BUILD_PROLIFIC=OFF`.

**Exit tests:** deterministic profile IDs, missing/admission-loss accounting, no wrong-context reuse, valid JSON, cache corruption and stale-revision invalidation; a guarded specialization whose assumption changes must deopt safely.

## 10. Orchestrate *all* Moonstone facilities without making each one mandatory

Create a small, versioned **Moonstone adapter layer** within On1x (suggested `include/on1x/moonstone/`, `src/moonstone/`), with one adapter per component and a single capability registry. Each adapter declares:

- actual LibStone public CMake target/header/API and checkout fingerprint;
- input/output On1x HIR/MIR type, source identity and contract/ABI provenance;
- conservative effect/ownership guarantees and unsupported operations;
- whether optional, needed for a compilation stage, build-time or runtime only;
- exact verification/test scope, resources/budgets, diagnostics and fallback behavior.

Example desired pipeline for a checked pricing kernel call:

```text
On1x call site
  -> ExoML CDecl v2 / contract consistency / target ABI evidence
  -> independent semantic review state (never auto-promoted)
  -> On1x elaboration: typed numeric/array/record and error interface
  -> Chisel + Wetstone usage/effect checks (unsupported => reject safe import)
  -> verified foreign-call MIR, Syren diagnostics and source provenance
  -> C11 (or later genuine native/VM backend), exact symbol/one-impl-unit link
  -> runtime sanitizer/ABI/error and lifetime tests
  -> optional Prolific observation; optional Plaid task only when safe
```

For a **pure ordinary function**, additionally allow opt-in Nerotypical Sequent optimization. For a possible optimizer rewrite, optionally ask Supersonic to synthesize candidates but retain the independent verifier. For revisioned relational analyses, delegate to Novostone as described in `BACKEND.md`. Do not infer cross-component capabilities from inclusion alone; each integration requires a real adapter and a differential/negative test.

### Contract-to-compiler interface (design only)

The minimum adapter concept (not existing source API) includes:

```text
ForeignImport {
  symbol, source_header, header_hash, target_abi,
  cdecl_identity, exocontract_identity, review_evidence,
  c_function_type, abi_layouts,
  parameters: [{type, signedness, extent, alignment, owner, borrow,
                invalidations, allowed_aliases}],
  result: {type, owned_or_borrowed, nullable, destructor},
  error: {classification, valid_outputs_on_failure, cleanup},
  effects: {reads, writes, allocates, frees, io, threads,
            suspends, unknown},
  concurrency: {thread_safety, thread_affinity, blocking, suspension_safe},
  link: {manifest_modules, implementation_switches, libraries},
  admission: {discovery, contract, review, abi, usage, runtime, reproducible}
}
```

Persist a deterministic, versioned serialized form of this descriptor and its *rejection reasons*. The actual on-disk ExoML contract remains authoritative; never create an incompatible substitute schema or remove information through a lossy ownership projection.

## 11. File changes expected in `mulib/on1x/`

The agent should modify existing files as necessary rather than blindly creating duplicates. Suggested additions:

```text
on1x/
  COMPLETION.md             # overall Shylock-ready completion requirements
  BACKEND.md                # LibStone IR, Novostone, native/VM completion
  MOONSTONE.md              # this Moonstone integration and trust specification
  include/on1x/moonstone/   # typed public adapter APIs as actually implemented
  src/moonstone/            # core/ExoML/Chisel/Syren/Supersonic/Plaid/
                            # Prolific/Nerotypical adapters
  contracts-exoml/           # existing source-reviewed v2 sidecars & evidence
  tests/moonstone/          # unit + negative + end-to-end component suites
  tests/foreign/            # C11 ABI/probe/mutating/error/failure fixtures
  examples/moonstone/       # actually compiling On1x examples
  cmake/                    # real dependency, target & installed-package wiring
  docs/                     # integration status, safety boundaries, manual
```

If an integration belongs in LibStone itself, do not silently alter the submodule. Provide a separate upstream patch/request and a compatibility test against both pinned and proposed versions. If no upstream change is required, keep integration strictly inside On1x.

## 12. Build plan, tests and trust boundaries

### Build profiles

1. **Minimal C11:** On1x compiler + its runtime + selected MuLIB headers, with LibStone generated/fallback frontend as already supported. This must work without optional Prolific/Lua/LMDB, Plaid, native codegen or Nerotypical runtime.
2. **Contract development:** enable existing `ON1X_WITH_EXOML=ON`, optionally `ON1X_WITH_EXOML_EXTRACT=ON`, and `ON1X_LIBSTONE_ROOT` pointing at the actual checkout. Build CDecl/contract tooling and run the pin/check/snapshot suites. Source/manual pins must be updated *only after real semantic audit*.
3. **Analysis:** opt-in Chisel/Syren/Supersonic/Nerotypical adapters and true public targets. Do not force nonexistent CMake aliases or install unregistered targets.
4. **Concurrent/profiling:** opt-in Plaid runtime and Prolific; record thread/libc/LMDB/Lua dependencies and only test on supported environments.
5. **Native/multi-IR:** opt-in external compiler subsystems as required by `BACKEND.md`; this must not break profile 1.

### Reference build commands (from `mulib/` root)

```sh
cmake -S on1x -B /tmp/on1x-moonstone-check \
  -DON1X_REQUIRE_LIBSTONE=ON \
  -DON1X_LIBSTONE_ROOT="$PWD/on1x/third_party/libstone" \
  -DON1X_WITH_EXOML=ON -DON1X_WITH_EXOML_EXTRACT=ON \
  -DBUILD_TESTING=ON
cmake --build /tmp/on1x-moonstone-check --parallel
ctest --test-dir /tmp/on1x-moonstone-check --output-on-failure

python3 on1x/tools/on1x-contracts.py lint
python3 on1x/tools/on1x-contracts.py build-plan json vec parsec
```

**Caution:** exact tools/build options/SDK prerequisites vary; `lint` is **not** ExoML Gate B. Running the shown CTest suite does **not** certify safe imports or all Moonstone tools. Run the documented SDK snapshot and admission tools, standalone component tests, sanitizer tests, and new end-to-end tests separately. Use the actual `ON1X_LIBSTONE_ROOT` if it differs. Disable Prolific when its optional dependencies are absent instead of silently enabling mocks.

### Independent acceptance matrix

| Area | Executed acceptance requirement |
|---|---|
| Core | source span, UTF-8/multiline, deterministic diagnostics, C ABI and exception containment |
| ExoML | canonical extracted CDecl/contract, target ABI probe, reviewed-vs-unreviewed gates, safe adapter rejection and genuine real-kernel tests |
| Chisel | move/borrow/reallocation/return/error/closure/invalidated borrowed view tests, sanitizer clean |
| Syren | structured correct source error, SARIF Unicode, validated vs rejected repair classification |
| Supersonic | bounded synthesis, overflow counterexample, independent proof/unknown/exhaustion distinction |
| Nerotypical | C11-vs-Sequent *pure subset* differential, verifier gates and cost budgets; effectful rejection |
| Plaid | C11 host ABI tests, cancel/drop, MPMC stress, TSAN where support exists, suspension-safe FFI |
| Prolific | context-aware trace, versioned cache, loss accounting, explicit guard/deopt if speculation used |
| Full pipeline | On1x Shylock source invokes real MuLIB forecasting and optimization kernels with typed errors and no illegal owner escape |
| Packaging | GCC/Clang strict builds, relevant ASan/UBSan/TSan runs, install/relocation consumer, no hidden vendor source pollution |

No result may be counted as passing when its test did not run. ASan/UBSan/TSan environment failures, nonportable target probes and missing optional packages must be documented as distinct statuses, not ignored or counted as green. Never suppress a failing test by disabling the feature while still claiming that feature is implemented.

## 13. Work packages in dependency order

**M0 — Pin and audit.** Compare the real submodule and upstream revisions, inventory all *seven* Moonstone subsystem trees plus Core, read active source/test/status files, inspect CMake exports and publish an accurate implemented/partial/missing matrix.

**M1 — Core and diagnostic bridge.** Stable source/IR IDs, SourceManager/Result conversions, Syren catalog and SARIF. Implement real compilation diagnostics with negative tests.

**M2 — ExoML foreign ABI path.** Preserve CDecl and v2 contract records; implement supported numeric/array/struct/enum/output parameter adapters with strict ABI checks. Provenance, rejection codes and review-state honesty are mandatory.

**M3 — Chisel/Wetstone safe callsites.** Intraprocedural then interprocedural ownership, alias/effect, branch cleanup and resource invalidation for selected pricing kernels. Do not emit “safe” on an unsupported analysis path.

**M4 — Nerotypical integration.** Optional pure On1x -> Sequent -> On1x verifier/optimizer bridge; no invented effect semantics. Integrate its target/packaging and isolated header tests.

**M5 — Supersonic assist.** Verified bounded candidate generation and Syren repair suggestions, strictly optional with distinct unknown/provisional/proven statuses.

**M6 — Plaid runtime + future async.** Start with explicit tasks, snapshot-only safe parallel pricing computation and C ABI; add real compiler async lowering only after ownership and cancellation correctness.

**M7 — Prolific and optimizer feedback.** Instrument actual compiler operations; integrate context-aware profile ranking and optional, correctly guarded speculation. Never turn profile advice into semantic proof.

**M8 — End-to-end acceptance.** Compile, link and run Shylock using the canonical MuLIB headers and actual On1x source. Re-run `COMPLETION.md` gates plus all Moonstone adapter tests under supported toolchains. Record reproducible commands, results, dependency revisions and remaining exclusions.

## 14. Definition of done and required agent reporting

An agent may claim **Moonstone-integrated On1x for Shylock** only when:

1. Real reviewed/admitted ExoML bindings needed for Shylock compile via the On1x typed API, **or** Shylock's executable-ready milestone explicitly uses marked, tested trusted adapters and separately declares that the safe-FFI milestone is still open.
2. Every relevant foreign call uses the correct ABI and error/ownership/effect lowering, with independent negative and runtime verification. Unreviewed contracts are never promoted to safe automatically.
3. Syren diagnostics and source provenance survive to the user; Chisel/Wetstone rejects unsupported unsafe resource behavior rather than masking it; there is an opt-in working pure Nerotypical bridge with documented bounds.
4. The supported Plaid and Prolific integrations, where enabled, run their *own* actual stress/profile/ABI tests; absence of optional systems does not stop minimal C11 + MuLIB applications.
5. The final run compiles and executes a real On1x Shylock fixture with forecasts, validation, optional demand/optimization, deterministic output and resource cleanup, without requiring the compile-time LibStone C++ runtime in the generated ordinary C11 process.
6. The compatibility and test report names every compiler/target actually executed, exact LibStone and MuLIB revision, contract evidence snapshot, all failures and exclusions. No placeholders, hardcoded demo success, fake native execution or prose-only implementation is accepted.

**The goal is not to make On1x depend on every Moonstone library at runtime.** The goal is to make On1x a well-verified compiler that uses *all relevant Moonstone capabilities* in their correct phase: Core for infrastructure; ExoML for declared foreign interfaces; Chisel for ownership; Syren for diagnostics; Supersonic for validated synthesis; Nerotypical for compatible functional IR; Plaid for real concurrency; and Prolific for observation-guided engineering. Unsupported use cases must remain explicitly unsupported until implemented and tested.
