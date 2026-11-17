# COMPLETION.md — Complete On1x for Shylock and MuLIB-Backed Applications

> **Audience:** autonomous coding agents working in `mulib/on1x/`.
> **Status:** implementation mandate and acceptance specification; not a claim of existing support.
> **Primary deliverable:** an honestly verified, statically compiled, safe-by-default On1x capable of writing **Shylock**, a price prediction, demand estimation, and pricing optimization engine built by composing MuLIB C11 kernels through LibStone ExoML semantic contracts.
> **Repository:** `https://github.com/Chubek/mulib` (`on1x/`, `include/`, `library/`, `MANIFEST.yaml`); LibStone: `https://github.com/Chubek/libstone` (`moonstone/exoml/`, related analyzers).

G4 continuation (2026-11-17): the bounded [cooperative fiber/channel
profile](docs/concurrency.md) compiles and runs real On1x `taskgroup`, internal
`ifiber<i64>` launch/join, i64 1:1 uni channels and bounded atomic `poll`
through C11 and existing MuLIB kernels. A bounded POSIX direct-exec `xfiber`
with process-group cancellation is executable; it is not the proposed portable
shell or typed process-stream model. Immediate pre-dispatch internal cancellation
is also executable; deadlines, running/blocked cancellation, async, native workers,
general resources and full concurrency/MIR semantics remain unsupported. This
does not advance the application or general-safe-FFI gates below.

M2 continuation (2026-11-17): [function-scoped defer and nominal Result
propagation](docs/defer-errors.md) now execute through C11. Defers snapshot
operands and run LIFO at function return, including typed `Err` returns; bounded
records/captures reuse existing RC cleanup. This supersedes older absence claims
below only for that profile. Using, general exceptions/effects, resource-bearing
results, trap/cancellation unwinding and general foreign error adapters remain
unsupported; no application/general-FFI gate is promoted.

Category continuation (2026-11-17): a native On1x-authored processor now expands
explicit, module-local scalar protocols/witnesses into ordinary checked selectors.
See [the exact profile](docs/category.md). This does not implement constrained
generics, associated types, signatures/functors, sealing, ownership auto protocols
or advance any general resource/FFI/application-completion gate below.

Core-syntax continuation (2026-11-17): intrinsic statement loops, for-C, ascending
i64 ranges and lexical exits execute through C11 and scalar MIR. Guarded
structural matching now has typed decision regions and C11 execution, plus MIR
for admitted scalar/tuple cases. The [profile](docs/core-syntax.md) preserves
lexical cleanup. The subsequent [value-loop slice](docs/value-loops.md) adds
typed loop results through the same regions/C11 and scalar/tuple MIR. Owning
lists, defer/error unwinding and concurrency remain absent. The later
[nominal-data increment](docs/nominal-adts.md) admits nonrecursive named records
and parameterized enums only; recursive owning layouts remain unsupported. This
does not complete any application/general-language gate.

## 0. Instructions to the agent: implement, test, and report

This file is an **execution specification**, not an invitation to produce a new roadmap in place of code. Inspect the current tree; implement the missing functionality in dependency order; add negative and integration tests for every new feature; run the relevant toolchains; and update the checked-in documentation with measured results. Do not stop after writing skeletons, interfaces, TODOs, mock backends, speculative contracts, or examples that cannot compile.

**Definition of done:** the Shylock-ready gate in §12 passes, with a real On1x application (not a C program masquerading as one), real canonical MuLIB headers, the real ExoML SDK where the build profile requires it, and a complete documented build. If a required dependency is absent or a gate fails, state the blocker and keep the corresponding capability marked **NOT IMPLEMENTED**. Do not falsify the status.

### Non-negotiable rules

1. **Preserve the language.** Do not break existing On1x grammar, accepted syntax, macro hygiene, evaluation order, ABI, generated outputs, or current examples. The user's established On1x syntax and Static FFI approach are architectural constraints. Implement already-specified features rather than redesigning the language. Make additive syntax changes only where a documented language feature requires them; record and test their compatibility. Prefer compiler-synthesized imported interfaces rather than inventing new syntax for contracts.
2. **Preserve the ownership split.** MuLIB implements C11 kernels; ExoML is the authority for C declarations, ABI discovery, and versioned semantic contracts; On1x owns type checking, resource-safe language APIs, effect/ownership enforcement, and C11 lowering. Do **not** create a parallel, lossy C type parser, a duplicate contract schema, or copied MuLIB algorithm implementations.
3. **Use canonical kernels.** Read `../include/mulib-*.h`, `../manual/`, `../MANIFEST.yaml`, and any relevant `../library/` support code. Do not assume an algorithm exists because of its name. Do not silently replace a kernel with an approximate reimplementation in an adapter.
4. **LibStone may be external.** The supplied checkout can be at `on1x/third_party/libstone` or elsewhere through `ON1X_LIBSTONE_ROOT`; some existing instructions also use the parent checkout's `third_party/libstone`. Detect the **actual configured root**; do not clone over, rewrite, or vendor a supplied checkout. Record its commit/hash. Treat it as read-only unless an explicitly authorized cross-repository task requires a patch. Propose any necessary LibStone changes separately.
5. **Fail closed.** Header discovery, an extracted CDecl, a successful ABI probe, an AI-written annotation, `verify`, or a review-status string alone is **not proof that a safe import is sound**. Never turn `safe_export` true without every corresponding gate. Never turn an unsupported call into an unchecked cast or fabricated type.
6. **Explicit escape hatches are unsafe.** Existing `extern fn` is an ABI-exact raw escape hatch. Maintain this distinction. A manually audited C shim can be a temporary **trusted adapter**, but mark it as such and test it; it does not become an ExoML-certified safe binding by declaration.
7. **Keep generated native programs small and standalone.** Runtime applications must not require ExoML, libclang, LibStone's interpreter, a C++ runtime for bindings, or the On1x compiler. Build-time tools may use those dependencies; emitted application code should remain C11 plus selected MuLIB kernels and the On1x runtime.
8. **One implementation unit.** All selected `MULIB_IMPL_*` switches must be defined in exactly one generated or dedicated C11 translation unit, with correctly resolved dependencies. Never define an implementation switch in ordinary consumers or multiple object files.
9. **Safety and numerical integrity before features.** Every foreign pointer, array length, result, error, allocation, and numeric conversion must be checked. A Shylock forecast or optimized price must never be produced from invalid or cross-currency input or from a training/test leak.
10. **Honest progress.** Track each feature as `unimplemented`, `partial`, `implemented`, or `verified`, linked to concrete tests, source paths and commands. Preserve prior verification evidence instead of replacing it with generic success claims.

## 1. Baseline: what is already implemented, and what is not

**Language-only M1 continuation:** `f64` values now have a real HIR/C11 path,
including checked conversions and user-declared raw C `double` calls. See
`docs/numerics.md` and `docs/capability-matrix.md`. Decimal literals and MIR v2 for
float/trivial-tuple values are now executable. A second M1 slice adds the bounded
value data model: structural records, closed sums with exhaustive `match`, fixed
arrays and owning snapshot slices as by-value C11 ([contract](docs/data-model.md));
numeric buffers/records, managed
aggregate MIR and general safe-import admission remain open. Reviewed scalar
adapters now cover signed/unsigned integers, `_Bool`, binary32 `float` and
binary64 `double`. Distinct u8/u16/u32/u64/f32 values now execute through
HIR-to-C11 with explicit conversions and suffixed literals; numeric scalar MIR v3
and independent type checking now execute as well. Broader targets and full
resource/aggregate/closure MIR remain open. This
does not satisfy the full numeric work package or promote any acceptance gate
below; historical reports and application-specific requirements are unchanged.

**Implementation continuation (2026-11-14):** scalar Core/MIR v1 now supports
verified round-trips, effect/CFG/initialization checks, reference evaluation and
optional MIR-to-C11 emission. This is a partial B1 backend foundation, not the
numeric/foreign binding work below. See `docs/backend-mir.md`,
`docs/shylock-readiness.md` and the new `VERIFICATION.md` continuation. Both
Shylock readiness labels remain **NO**; no acceptance checkbox is promoted by MIR
tests alone.

At the time this specification was authored (2026-11-14), repository documentation reports a tested **subset**, not a complete On1x compiler. Check the current checkout instead of assuming these statuses remain unchanged.

### Preserve working subsystems

- Optional Nerotypical/Satie structural verification of concrete HIR, with
  independent type obligations and closed rejection on unsupported/unknown cases;
  see `docs/type-safety.md` for the source SDK patch and supported subset.
- Optional Chisel scalar/JSON ownership CFG checks and a compiler-side external
  resource protocol API; see `docs/memory-safety.md` for the supported boundary.
- Backend continuation (2026-11-15): opt-in B3 scalar Datalog/CCDB analyses and
  transactional B4 checked folding, CFG cleanup and dead pure-value/phi passes;
  see `docs/backend-analysis.md`, `docs/backend-passes.md` and `VERIFICATION.md`.
- ASDL/Aurocks-generated scanning, independent bootstrap fallback, scanner parity, bounded token/source handling.
- Parser and typed C++ HIR for the implemented core; imports, first-class functions, immutable closures, tuples/patterns, recursion, control flow, checked `i64` arithmetic.
- Value-restricted polymorphism and explicit universals in the documented implemented subset.
- Hygienic syntax macros, PPX, their native On1x-authored tooling, and typed closed-code AOT staging.
- Deterministic C11 emission, runtime/linking, and existing real MuLIB JSON demonstration.
- ExoML CDecl extraction, lossless transport, v2 contract parsing/checking/pinning,
  evidence snapshots, narrow raw typed-scalar adapters, and the executable
  reviewed scalar-only four-gate admission profile.
- Existing tests and their published verification matrix in `VERIFICATION.md`.

### Known high-priority gaps

**Standard-library continuation (2026-11-15):** bounded raw scalar adapters for
JSON/YAML/TOML/INI, BLAKE2s/BLAKE3, POSIX process helpers and numeric-IPv4 sockets
now have automatic one-TU kernel linkage, status tuples, binary-hex reception,
allocation-failure tests and real Exotract archive consumers. Persistent helpers
and fifteen native PPXs have executable regressions. These are the documented
trusted-adapter subset, **not** Gate C review or general safe foreign admission;
f64/records/slices, foreign-owner lowering and Shylock's numerical gates below
remain open. See `stdlib/_bridge/README.md` and `VERIFICATION.md`.

- No general safe On1x foreign-import backend: `on1x-bind emit-safe` now admits
  only complete reviewed signed/unsigned-integer/`_Bool`/binary32-`float`/binary64-`double`/void-result bundles after profile, Chisel and
  executable target gates; every pointer/resource/error/callback profile still
  fails closed.
- `emit-raw` handles signed/unsigned integers, `_Bool`, binary32 `float`, binary64 `double` and void results; it rejects other floating formats, pointer/length arguments, aggregates, enums, output buffers, callbacks, and important ownership/error semantics.
- No integrated general `Result`/`Option` and resource-safe MuLIB API, beyond the limited fail-fast JSON facade.
- No general foreign-owned buffer/handle lifecycle, interprocedural borrow checking, or Chisel/Wetstone usage/effect proof.
- No completed nominal/parameterized ADTs, modules/functors, full callbacks, effect inference, or Plaid concurrency lowering. Guarded/refutable structural matching now executes in the bounded profile (`docs/core-syntax.md`); owning lists, recursive constructors and general resource matching remain closed. Existing by-value records/sums/arrays/slices retain the `docs/data-model.md` restrictions.
- The C11 backend works for the supported subset; native and VM backends are not required for this task and must not block it.

**Read first:** `README.md`, `VERIFICATION.md`, `docs/roadmap.md`, `docs/contracts.md`, `docs/architecture.md`, `docs/language.md`, `docs/closures.md`, `docs/tuples.md`, `docs/staging.md`, `contracts-exoml/catalog.json`, `contracts-exoml/obligations.json`, `tools/on1x-bind.cpp`, `tools/on1x-contracts.py`, `src/{parser,type,infer,sema,codegen,primitives}.cpp`, `runtime/`, `tests/`, and the canonical ExoML `Contract.hpp`, `CDecl.hpp`, `Extract.hpp`, `StaticFFI.hpp` and schema in LibStone.

## 2. Required application capability: what Shylock needs

Shylock is a real **On1x program** that composes selected MuLIB kernels. It is *not* a reason to duplicate them in On1x. Support the following independent application layers:

| Shylock layer | Representative canonical kernels | Requirement for On1x |
|---|---|---|
| Acquisition / formats | `mulib-csv.h`, `mulib-json.h`, HTTP/HTML/scraping kernels as applicable | checked byte/text I/O, owned buffers, parse errors; offline CSV/JSON first |
| Money / comparability | `mulib-money.h`, `mulib-price-observation.h`, `mulib-price-match.h`, `mulib-price-normalize.h` | explicit minor units, currency/precision, owned/copy-only records |
| History / time series | `mulib-price-history.h`, `mulib-price-timeseries.h` | safe handles, slices, invalidation-safe borrows, time-bucket flags |
| Statistics | `mulib-statsproc.h`, `mulib-statsprice.h`, `mulib-price-confidence.h` | `f64` arrays, parameter checks, structs and recoverable errors |
| Historical prediction | `mulib-histprice.h`, `mulib-price-forecast.h` | `f64` arrays; algorithm enums; fixed-layout result aggregates; no-data / failure handling |
| Ensembles | `mulib-price-ensemble.h` | borrowed arrays, independent out-parameters, model errors and comparison |
| Competition | `mulib-competprice.h`, `mulib-price-monitor.h` | arrays of C records, option/config records, checked results |
| Demand / causal analysis | `mulib-price-demand.h`, `mulib-price-causal.h` | paired numeric arrays, fitted-model records, uncertainty / identification diagnostics |
| Optimization | `mulib-priceopt.h` | demand-model and constraint records, enums, scenarios, candidate arrays, safe results |
| Application | On1x stdlib, modules and CLI | compositional modules, loops, functions, I/O, serialization and explicit fallback policy |

Do not claim there are “60–100 price kernels” unless enumeration of the current repository actually verifies that number. First inventory the exact kernels and distinguish direct pricing functions from supporting kernels in `include/` and `library/`. Record their public signatures, implementation switches, dependency closure and contract readiness in a machine-readable catalog.

### Required outputs from one end-to-end run

A Shylock smoke-test executable must read a versioned local fixture containing: a product identifier, currency/precision, timestamps, historical prices, contemporaneous competitor observations, and optionally costs and own-price/sales observations. It must then:

1. Validate and normalize price and product identity; reject mixed currencies/uncomparable offers and invalid monetary values.
2. Build a time-series with explicit missing-observation flags; never silently equate an imputed value with an actual observation.
3. Produce a baseline forecast and at least two **distinct** historical forecasts from canonical MuLIB kernels.
4. Perform strictly time-ordered holdout/rolling-origin evaluation with no leakage; show MAE or equivalent, rejected-model reasons and comparison to the naive baseline.
5. Optionally blend validated forecasts; label inter-model dispersion accurately (it is **not** a calibrated prediction interval).
6. Estimate demand only when valid own-price/sales information exists; otherwise return `insufficient_data` rather than an invented elasticity.
7. Optimize a recommendation only when costs, demand assumptions and price constraints are supplied; otherwise return a forecast without a fabricated “optimal” price.
8. Emit machine-readable output with model/kernel versions, input provenance, selected method, forecast horizon, units, error metric, and any rejection/fallback reason.

## 3. Work package A — reproducible baseline and source-of-truth inventory

Before changing compiler code:

- Run generated ASDL/Aurocks and fallback configurations, with and without ExoML discovery as supported. Capture executable versions, LibStone location/revision, compiler triples and baseline `ctest` results. Preserve sanitizer coverage where available.
- Inventory APIs and ABI requirements from real MuLIB headers and `MANIFEST.yaml`: C type graph, enum domains, structs/unions, callbacks, pointer+length pairings, all `MULIB_IMPL_*` definitions, `-lm` and other real link flags.
- Cross-reference every selected kernel with its manual/examples/tests. Mark ambiguity and missing documentation for resolution, not assumption.
- Create a Shylock-specific **capability matrix** in `docs/shylock-readiness.md` showing each selected public C function, discovery status, ExoML contract state, ABI lowering, ownership proof, error adapter, and executing regression test.
- Establish stable golden tests on the existing On1x language; do not “fix” failures by weakening tests or removing supported features.

**Acceptance:** checked-in inventory and reproducible baseline; all preexisting passing tests remain passing, or a specific reproducible preexisting/environmental failure is documented.

## 4. Work package B — numeric and C ABI completeness

This is the first unavoidable backend blocker for Shylock. Implement **real** value semantics and C11 codegen for:

- `f64` (at minimum), with parse/type/infer/lower/runtime support, C `double` ABI, correct NaN/Inf handling, and deterministic printing/serialization. If adding `f32`, implement independently rather than aliasing it incorrectly.
- Signed and unsigned fixed-width C integers (`i8/i16/i32/i64`, `u8/u16/u32/u64`) and `usize`/`isize` *as required by selected kernels*; checked conversions from On1x numeric values. Do not assume `size_t` is `i64` or that `long` is 64 bits across targets.
- `bool` and C `_Bool`; C `int` success codes with correct ABI; enums with validated domains; fixed-layout structs returned through verified generated adapters; opaque handles without exposed arbitrary pointer arithmetic.
- Immutable numeric slices and mutable, bounded output buffers in the typed HIR; represent actual length, element type, alignment, owner identity and extent. Validate `n <= length` and multiplication/offset overflow **before** entering foreign code.
- C out-parameters and pointer-to-struct parameters with exact calling conventions; stack-allocated temporary outputs where appropriate. Stage results until a success code verifies they are initialized.
- Checked interoperation between integer **minor-unit money** and floating-point model inputs. Never conflate minor units with major currency units or silently truncate/overflow.
- Explicit foreign-call evaluation order and failure propagation consistent with existing On1x left-to-right evaluation semantics.

Use the compiler's own typed HIR and generated C11 shims. A safe binding must never be implemented by casting a function pointer, forging a C layout, passing a managed language object as an unverified C struct, or exposing an unbounded `void*` to ordinary code.

**Tests:** exact C11 prototypes and `_Static_assert`/`_Generic` checks where appropriate; native layout probes compiled with the target toolchain; 32-/64-bit (where toolchains permit); overflow and nonfinite edge cases; signed/unsigned conversion rejections; ASan/UBSan; differential native C vs emitted On1x for real kernel calls.

**Acceptance:** at least `mulib_histprice_ema`, `mulib_histprice_holt`, `mulib_statsprice_estimate`, `mulib_price_ensemble_blend`, and `mulib_priceopt_optimize` are reachable through checked *typed* interfaces without unsafe type punning or user-visible C pointers, subject to the safe-import/adaptor trust distinction below.

## 5. Work package C — ExoML-to-On1x static binding generator

Retain the **canonical ExoML v2** sidecar model in LibStone. Add or extend `on1x-bind` in-place, using public ExoML APIs. Required stages:

```text
canonical MuLIB .h + compilation context
       |
       v
ExoML CDecl extraction (complete, target-specific)
       + ExoML v2 semantic-contract sidecar
       |
       v
shape/provenance validation + independent contract review gate
       |
       v
ABI/layout/enum/calling-convention checks + capability classification
       |
       v
ownership/effect/borrow obligations + On1x checker integration
       |
       v
verified typed interface + deterministic C11 adapter + evidence manifest
       |
       v
On1x import -> generated C11 program -> linked canonical MuLIB kernels
```

### Build an actual binding path, not just checked JSON

- Keep and extend `extract`, `normalize-decl`, `pin`, `check`, `explain`, `verify`, `emit-raw`. Preserve their existing exit behavior and fail-closed modes unless explicitly versioned.
- Implement a real `emit-safe` **only for supported, reviewed, usage-checked profiles**. Unsupported or unreviewed contracts must produce explicit per-function errors. It is acceptable to support a narrow sound subset first; it is not acceptable to label an unsafe, manually asserted import as safe.
- Generate **both** a typed On1x-facing module and ABI-correct C11 glue, or a compiler-consumable typed interface artifact whose import requires no new user-facing grammar. Ensure its public API is stable under backend replacement.
- Generate a binding/implementation manifest with exact header/manual hashes, ExoML schema, source/toolchain identity, target triple, ABI fingerprint, selected functions, unsafe exclusions, source paths and generated outputs.
- Use explicit immutable, borrowed, mutable, owned and consumed resource semantics from the full v2 contract, **not** only its lossy legacy ownership projection.
- Handle errors as tagged safe results, not `abort()` or blind assumption of output initialization. Preserve kernel-specific error contracts (boolean success, sentinel, `errno`, null, failure-unchanged outputs, cleanup-on-failure).
- Enforce contract preconditions for finite numeric inputs, ranges, aligned buffers, pointer lengths, and conditional transfers; use runtime checks for value-dependent properties not statically provable.
- Never let build-time ExoML runtime/plugin state leak into the generated application. Static FFI means emitted direct calls, not runtime interpretation or dynamic symbol guessing.
- Reject varargs and unsupported callback/union/packed/bitfield cases unless there is a separately verified, explicit adapter. Do not weaken checks just because a feature exists elsewhere in LibStone.

### Trust gates (all must hold for a function to become `safe`)

1. **Gate A — discovery:** complete compiler-derived CDecl for the intended target and compilation flags; no omitted declarations or guessed layouts.
2. **Gate B — contract consistency:** ExoML v2 schema/version, exact signature/indices, valid ownership/lifetime/error/effect semantics and pinned provenance.
3. **Gate C — review:** independent human or separately authorized trusted review of actual C implementation and manual; `reviewed`/`verified` cannot be conferred by the coding agent merely changing metadata.
4. **Gate D — ABI:** generated C11 checks compile on the target, including alignment, member offsets, enum representations, integer widths and function-pointer signature comparisons where valid.
5. **Gate E — use-site safety:** On1x's own analysis proves or inserts enforcement of borrow regions, exclusivity, consumption, output initialization, cleanup and effect restrictions for each call.
6. **Gate F — runtime verification:** positive/negative differential tests, allocation-failure testing, adversarial lengths/nulls, sanitizer runs and determinism/provenance checks pass.

When review evidence is unavailable, keep generated exports marked **trusted/unsafe** and still implement usable tested adapters for bootstrapping. A trusted wrapper may be high quality, but must not be misrepresented as an independently reviewed safe import. The **Shylock executable milestone** may use these explicitly marked adapters; the **safe-import milestone** may not.

## 6. Work package D — ownership, lifetimes, effects and recoverable errors

### Ownership and borrow checking

Implement a coherent, testable resource model for FFI values in On1x's HIR and C11 backend:

- Unique foreign-owned handles with exact destructor, once-only drop and correct behavior on normal return, early return, loop exit, break and conditional paths.
- Immutable borrows shared read-only; exclusive mutable borrows; no mutable alias with a live immutable borrow; no borrow escape beyond its owner's or buffer's lifetime.
- Explicit invalidation on owner drop, move, vector growth/reallocation, item removal, history mutation, and any contract-declared invalidating operation.
- Conditional ownership transfer by *actual* success/failure outcome; correct cleanup of partially initialized and partially transferred aggregates.
- Checked ownership of temporary numeric buffers and C strings with length/encoding/terminator differences. Never assume byte strings are NUL-free or null-terminated unless their contract proves it.
- No use-after-free or double-free across closures, imports, nested calls and early exits. For mutable/resource captures, either implement sound capture/lifetime lowering or reject the capture with a precise diagnostic.
- Interprocedural summaries and foreign-call effects; unknown effects stay conservative and may block optimizations or concurrent use.

Integrate LibStone **Chisel** and **Wetstone** where their actual public APIs support the required analyses. Preserve a well-defined compiler-local analysis path if integrations are unavailable, and document the unproven cases. Do not claim full region or abstract-interpretation proof just because a library is linked.

### Recoverable error types

Provide typed recoverable errors without changing existing supported On1x syntax. Implement the documented/approved `Option<T>` and `Result<T,E>` abstractions with actual representation, construction, propagation, safe inspection and destruction. If full ADT syntax/matching is still incomplete, bootstrap these through compiler-generated interfaces and a verified internal lowering; do **not** make Shylock rely on an abort-only facade.

Model at least these outcomes:

- invalid argument / nonfinite sample / overflow / allocation failure;
- insufficient data / invalid time series / incompatible money units;
- unrecognized model / unsupported binding / ABI mismatch;
- binding-contract violation / ownership violation detected before foreign invocation;
- infeasible optimization constraints / unavailable causal evidence;
- data-source parse, I/O and schema failures.

Prefer exact error codes and structured diagnostics. Preserve existing source locations and stable CLI exit statuses. Unexpected fatal internal errors remain distinct from user-facing recoverable kernel errors.

**Acceptance:** tests deliberately trigger every category; no uninitialized outputs, leaked temporary buffers, incorrect destructor calls, false `Ok`, or hidden process termination.

## 7. Work package E — make the On1x application layer genuinely usable

Target **language usefulness for Shylock**, not unrelated research features first:

- General, repeatable function and import composition across many `.on1x` modules, deterministic initialization and symbol resolution, and useful private/public module organization without breaking existing imports.
- Type-safe collections or compiler-backed library abstractions for numeric series, immutable views, observation records, model specifications, scores and errors. No arbitrary raw pointers in ordinary application code.
- A practical file/stream, CSV and JSON API with explicit ownership and recoverable errors. Prefer binding canonical MuLIB kernels, not adding an unrelated runtime library. Offline ingestion is mandatory; live web ingestion is optional for the first milestone.
- Stable typed model descriptions, parameter validation, forecasting interfaces, backtesting interfaces and output serialization. Avoid a giant hardcoded algorithm switch.
- Necessary named records/variants and exhaustive matching **when required** by the language's already-established design; do not require full functors, higher-kinded types, or an alternate native backend for a pricing MVP. Compile unsupported constructs with precise diagnostics rather than pretending to implement them.
- Preserve value restriction, hygiene, source mapping, expression evaluation order, and generated C11 resource cleanup.
- Efficient large-series operation: avoid copying `f64` vectors on every call; make borrowing and ownership visible to the checker. Add bounded memory/time behavior and verify linear vs quadratic work for selected algorithms.

The application must orchestrate actual MuLIB calls. Forecasting, statistical fitting and numerical optimization must remain canonical C kernels unless a **new** capability demonstrably has no suitable kernel and is explicitly scoped as an On1x algorithm.

## 8. Work package F — Shylock contract profiles and generated modules

Add v2 ExoML contract sidecars and catalog entries for the exact public declarations that are used. Start with these concrete profiles; use names that match the actual repository and generated interface naming conventions:

1. `statsproc`, `statsprice`, `histprice`.
2. `price_forecast`, `price_ensemble`, `price_confidence`.
3. `price_demand`, `priceopt`, `competprice`.
4. `money`, `price_observation`, `price_normalize`, `price_history`, `price_timeseries`.
5. `price_match`, `price_monitor`, `price_causal`.
6. Supporting input/storage kernels only as needed for real Shylock fixtures.

For each profile document: versioned canonical header, all selected public functions and reasons for exclusions, complete relevant types, enum values, pointers and lengths, owned/borrowed field relationships, output-valid-on-failure rules, allocator/destructor family, effects/thread safety, data-preconditions, target ABI checks, expected implementation macro closure and executable tests.

**Critical concrete cases:**

- `mulib_histprice_ema(const double*, size_t, double, size_t, mulib_histprice_forecast_t*)` — immutable `f64` input, result struct, error-handled out parameter.
- `mulib_histprice_holt(...)`, `mulib_histprice_kalman(...)`, `mulib_histprice_ar1(...)` — model-specific parameter validation; do not invent variance if a model reports none.
- `mulib_statsprice_estimate(const double*, size_t, mulib_statsprice_method_t, double, double*)` — bounded borrowed data, actual enum, scalar output only on success.
- `mulib_price_forecast(...)` and `mulib_price_forecast_mae(...)` — uniform spacing, horizon and sample restrictions.
- `mulib_price_ensemble_blend(const double*, const double*, size_t, double*, double*)` — two independent borrowed input arrays, two distinct valid out pointers, no output commit on failure.
- `mulib_price_demand_fit(...)` — paired arrays and structurally checked fitted model.
- `mulib_competprice_recommend(...)` — borrowed array of correctly laid-out rival structs and explicit config/result aggregates.
- `mulib_priceopt_optimize(...)`, `mulib_priceopt_scenarios(...)`, `mulib_priceopt_candidates(...)` — correct kind/objective enums, constraints, scenario weights, size and tick validation.
- `mulib_price_history_add(...)` and range access — owned history with invalidated borrows after mutation/reallocation.

The contract generator must derive implementation switches/dependencies from `MANIFEST.yaml`, with manual confirmation against the module's documented supported build profile. Never enable optional implementation branches or unrelated kernels by guessing.

Deliver a typed facade with stable names and explicit input/output ownership. Suggested *conceptual* shape (not a claim that this exact syntax currently parses):

```text
MarketHistory -> Result<Series, DataError>
Series + ForecastConfig -> Result<Forecast, ForecastError>
Series + BacktestConfig -> Result<ModelScore, BacktestError>
Forecasts + OutOfSampleErrors -> Result<Ensemble, ModelError>
PriceSalesPairs -> Result<DemandModel, EstimationError>
DemandModel + Cost + Constraints -> Result<Recommendation, OptimizationError>
```

Generate On1x-facing definitions from source-backed contracts. Do not author unchecked `extern` prototypes pretending that `double*` is `i64`, or make user code manually allocate/`free` raw C structures.

## 9. Work package G — build system and distribution

Enhance `tools/on1x-build.py`, CMake and `on1x-bind` as needed, without breaking existing CLI arguments. Supply a reproducible process that:

1. Resolves the configured LibStone checkout and canonical parent MuLIB repository.
2. Produces/rechecks ExoML declaration and contract artifacts and their evidence lock.
3. Generates checked On1x modules and C adapters in a **build directory**, not by overwriting handwritten sources.
4. Compiles the On1x application to readable portable C11 and a small On1x runtime.
5. Emits **one** MuLIB implementation translation unit containing exactly the chosen transitive `MULIB_IMPL_*` switches, selected modules and no duplicate definitions.
6. Links the correct C11 implementation and required system libraries (e.g. `-lm` when documented); keeps build-time compiler/ExoML dependencies out of the executable.
7. Produces a dependency/build manifest and reproducible build log with exact command lines (safely executed, no shell interpolation of untrusted text).
8. Supports `--check`, `--emit-c`, and an executable build for the new profile; validates staged paths and refuses incompatible stale contract locks.

Use incremental caching only when source hashes, target ABI, compile arguments, manifest, contract and generator versions all match. Revalidate admission evidence on cache hits. A generated application must be linkable with both GCC and Clang on a supported Linux target.

**Acceptance:** clean build of a nontrivial multi-file Shylock program from an otherwise fresh checkout with the declared dependencies. No hand-edited generated files, undeclared library dependencies, or runtime `dlopen()` masquerading as static binding.

## 10. Work package H — correctness, diagnostics, security and performance

For each new feature create a positive test, a compile-time rejection test, and a runtime/failure-path test. Put integration fixtures under `tests/` and keep runtime-generated artifacts out of the source tree.

### Required regression matrix

| Test family | Required evidence |
|---|---|
| Existing language | Full preexisting CTest suite, scanner generator/fallback parity |
| Numeric types | finite and nonfinite values, rounding, overflow, integer width, signedness, ABI equivalence |
| CDecl/ABI | native target signature, enum/layout/offset/align checks; deliberate ABI mismatch must fail |
| Contract admission | unreviewed, stale, tampered, incomplete, missing-lifetime, unsupported and contradictory contracts fail closed |
| Ownership | lifetime escape, aliases, double-drop, use-after-move, reallocation, early return, cleanup-on-error |
| Arrays | null/zero-length combinations, excessive counts, alignment, overflow, aliased out buffers |
| Error adapters | output unchanged on failure, partially initialized output, allocation injection and errno isolation |
| Generated code | compile with GCC/Clang `-std=c11 -Wall -Wextra -Wpedantic -Werror` |
| Sanitizers | ASan/UBSan for compiler and emitted programs; LSan/Valgrind when available |
| MuLIB differentials | same fixtures through direct C API and generated On1x; compare under explicit tolerances |
| Statistics | deterministic input, no train/test leakage, correctly labeled errors and uncertainty |
| Optimization | hard price bounds, unit margin, tick grid, infeasible constraints, invalid demand inputs |
| Robustness | fuzz bounded CDecl/contract/source/CSV/JSON inputs; reject malicious paths and oversized artifacts |
| Reproducibility | two clean builds of equivalent inputs have matching normalized artifacts and output |

Add deterministic property/metamorphic tests where meaningful, e.g. permutation invariance of cross-sectional estimators, unchanged output on rejected MuLIB calls, explicit currency/precision mismatch rejection, and deterministic forecast results for fixed ordered histories. **Never** use statistical tests to claim forecast accuracy on unseen real-world data without an actual evaluation.

### Diagnostics

Every refusal must identify the API, source location or contract function, actual type/owner/ABI, violated rule, and actionable remediation. Examples include:

- `cannot pass borrowed slice: owner may be dropped before C call returns`
- `mulib_price_ensemble_blend: two output regions must not alias`
- `mulib_histprice_ema: expected finite f64 samples and alpha in supported range`
- `safe import denied: ExoML contract exists but independent semantic review is missing`
- `ABI mismatch: sizeof(size_t) differs from the recorded target contract`

## 11. Optional, post-Shylock On1x completeness track

Do not make these blockers for the Shylock-ready gate **unless a core safety invariant requires them**. After the minimum engine passes, continue On1x toward its larger ML-language vision:

- Fully realized named records, algebraic data types, exhaustive pattern matching, and practical module abstraction/functors (consistent with the established syntax).
- More general Hindley–Milner typing, stronger effect inference and useful polymorphism without unsafe polymorphic FFI.
- Rich immutable/mutable collections, a fully owning string type with length-aware foreign interoperability, structured I/O and serialization.
- General safe callback bindings with explicit synchronous vs retained lifetime/unregister semantics; only when real contracts permit them.
- Chisel/Wetstone analyses integrated into compilation with reproducible diagnostics and proven coverage.
- Native/VM lowering and advanced parallelism/Plaid support **after** the stable C11 backend and foreign resource model are mature.
- Property-based, fuzz, benchmark, multi-platform and packaging verification sufficient for the broader On1x project.

If a future feature would require a breaking grammar or ABI change, do not slip it into this work. Document the conflict and request a separate design decision. This file authorizes completion of the **existing intended** language, not replacement with a different language.

## 12. Mandatory Shylock-ready acceptance gate

The project is **NOT DONE** until **all** of the following are demonstrated:

- [ ] Fresh configuration/build of On1x with the required LibStone SDK at an explicitly configured root, and independent fallback configuration.
- [ ] Preexisting On1x tests still pass on relevant configured toolchains; all new tests pass; precise exceptions are documented.
- [ ] On1x supports correct `f64` semantics and ABI, required integral widths/conversions, enum validation, checked foreign buffers/slices and fixed-layout C results.
- [ ] The selected real MuLIB pricing kernels are callable through **typed**, safety-checked APIs or individually declared, audited **trusted adapters**; no forged raw-pointer ABI.
- [ ] An actual ExoML v2 pricing-contract inventory, complete extraction, admission reports, source pins and review-state tracking exists. Safe exports remain unavailable until all safe gates pass.
- [ ] On1x has non-aborting, typed error paths and correct cleanup for successful/failed calls, moves, returns, loops and relevant buffer mutations.
- [ ] The C11 backend emits code that compiles/links independently of LibStone and ExoML with exactly one MuLIB implementation unit.
- [ ] A real **multi-module `.on1x` Shylock integration program** reads a local fixture, constructs a validated price history, invokes baseline + historical forecast kernels, backtests without leakage, optionally blends, and emits structured deterministic output.
- [ ] A second test with valid demand/cost/constraints invokes a canonical MuLIB pricing optimizer and returns a feasible result; invalid/infeasible inputs generate structured errors, not prices.
- [ ] The corresponding direct C integration returns matching outputs within documented numerical tolerances; negative cases, sanitizer and allocation-failure tests pass.
- [ ] `docs/shylock-readiness.md` includes exact verification commands, tested target/compiler identities, kernel/contract matrix and any known unsupported profiles.
- [ ] `VERIFICATION.md`, `docs/roadmap.md` and `README.md` honestly reflect what actually changed; no fabricated safety proofs, benchmark claims or “complete” statuses.

**Two separate final labels must be reported:**

- **`SHYLOCK-EXECUTABLE-READY`** — On1x and real MuLIB kernels can execute the above application using approved typed interfaces and, if necessary, explicitly identified trusted adapters; the backend and runtime are usable, but this alone does **not** assert independently reviewed safe FFI.
- **`SHYLOCK-SAFE-FFI-READY`** — in addition, every invoked foreign API used in the tested safe profile passes Gates A–F with independent review evidence, compiler ownership checks and generated safe interfaces; the integration no longer relies on unchecked/user-authored FFI for those APIs.

Do not conflate these labels. If only the first is met, document all remaining gaps to the second.

## 13. Exact working order and required deliverables

Execute in this order, with incremental green builds and committed evidence after each step:

1. Baseline + inventory (`docs/shylock-readiness.md`, test plan and source/ABI matrix).
2. Numeric/ABI types (`src/type.cpp`, `src/infer.cpp`, `src/sema.cpp`, `src/codegen.cpp`, runtime, grammar/lexer only as required without compatibility break).
3. ExoML interface generator and tested C adapters (`tools/on1x-bind.cpp`, related admission helpers, generated-module lowering).
4. Ownership, effects, error types and cleanup (`src/`, `include/on1x/`, `runtime/`, and compatible LibStone analyzer integration).
5. Pricing contract profiles (`contracts-exoml/`, catalog, obligations, review evidence and tests).
6. Reproducible implementation-unit generation and build workflow (CMake, `tools/on1x-build.py`, generated output/manifest tests).
7. Multi-module Shylock reference application and golden/differential tests (under `examples/shylock/` and/or `tests/shylock/`; keep a separate production `../shylock/` project optional).
8. Full GCC/Clang, generated/fallback, ExoML-enabled, sanitizer, failure-injection and reproducibility verification.
9. Update docs and present the two honest readiness labels plus exact unresolved blockers.

### Recommended reference fixture structure

Use actual schema files and program-generated test fixtures, but ensure at least:

```text
on1x/tests/shylock/
  data/
    historical_prices.csv
    competitor_prices.csv
    sales_by_price.csv
    invalid_currency.csv
    sparse_history.csv
    infeasible_constraints.json
  programs/
    market_forecast.on1x
    demand_optimizer.on1x
    invalid_borrow.on1x
    invalid_ffi_signature.on1x
  oracle/
    direct_c_reference.c
  test_shylock.py
```

Tests must make real calls to `mulib-histprice`, `mulib-statsprice`, `mulib-price-ensemble`, and `mulib-priceopt` in the canonical checkout. Add further kernels incrementally; do not demand an unsafe 100-kernel mass export before the core works.

### Final report format

At completion, produce a concise technical report with:

1. Implemented compiler features (source paths and public API changes).
2. Exact kernel contract/profile coverage and safe/trusted/unsupported classification.
3. Build/test commands, toolchain and SDK revisions, pass/fail counts, sanitizer results and numerical oracle outcomes.
4. A reproducible `on1xc`/build-tool command that produces and runs the reference Shylock executable.
5. Any remaining known limitations, with severity and next implementation steps.
6. The two readiness labels from §12, asserted only when justified.

**Success criterion:** Shylock can be written as On1x business logic composing independently implemented MuLIB kernels. On1x's backend does not have to be “finished in every possible sense” first, but its C11 backend, trusted semantic boundary, resources and numeric ABI **must** be real, tested, reproducible and honest. Continue beyond that only without compromising this gate.
