# MIGRATION_OF_FFI

**Agent execution specification — On1x / MuLIB**  
**Required parallelism:** exactly 12 implementation workers, coordinated by the supervising agent.  
**Input:** `FFHandler.zip` (FFIHandler v0), the live MuLIB checkout, and its pinned LibStone dependency.  
**Objective:** replace the On1x-specific KernelBind and standalone foreign-contract architecture with one LLSynth-defined FFIHandler subsystem under `on1x/hir/FFIHandler`.

## 0. Instructions to the supervising agent

Implement this migration, not merely a design document. Read this entire file, the supplied archive, repository instructions (`AGENTS.md` and applicable nested instructions), and current source before delegating. Launch the twelve workers defined in §5. Use independent worktrees or equivalent isolated branches and build directories. The supervising agent is the coordinator and integrator, not a thirteenth implementation worker. If your orchestration environment cannot actually start twelve workers, report that limitation and execute the same twelve work packages in dependency order; never claim nonexistent workers or fabricated results.

The end state is a single declarative foreign interface system, not FFIHandler layered over KernelBind indefinitely. Delete the superseded On1x-only contract and KernelBind implementations after replacement parity has been demonstrated. Preserve semantic checks, ABI checks and explicit native trust policy. Do not replace substantive implementations with mocks, TODOs, documentation, generated-looking hand-written files, or permissive stubs.

Work on a dedicated migration branch. Do not push, force-push, alter the user's uncommitted work, or update the LibStone submodule pin without explicit authorization. Do not modify MuLIB public C APIs merely to make the migration pass. If a genuine upstream blocker exists, isolate it and produce a minimal reproducer. Keep completed improvements usable even when another work package is blocked. Report unfinished scope accurately.

## 1. Starting evidence and baseline

`FFHandler.zip` is a useful seed, **not a qualified production implementation**. Its supplied v0 contains:

- `on1x/hir/FFIHandler/FFIHandler.llsynth`, `tools/ffic.py`, `ffi.cpp`, CMake wiring, and a scalar descriptor interface in `on1x/include/on1x/ffi_scalar.hpp`;
- `on1x/stdlib/ffi/mulib/clamp.ffi`, covering the three existing clamp functions;
- `apply.py`, an anchored edit installer for `hir/CMakeLists.txt`, `include/on1x/kernel.hpp`, `src/loader.cpp`, and `src/codegen.cpp`;
- standalone GCC/Clang tests, documentation, and logs.

The archive reported 567 forwarding comparisons per compiler and 14 matched source edit anchors. Its tests used a **test-only grammar projection**, not actual LLSynth output. The full On1x build, real generated parser, reviewed-import path, Cephyr integration, and project driver were not tested by the archive. The worker team must close those gaps. The archive's `verification/` records historical evidence, not current qualification.

Before applying anything:

```sh
# In the MuLIB checkout; read-only inventory first.
git status --short
git rev-parse HEAD
git submodule status --recursive
```

Read `on1x/hir/README.md`, `hir/processor/INTERFACE.md`, `hir/processor/CMakeLists.txt`, `hir/MEMGraph/ADAPTER.md`, `docs/kernel-bind.md`, `docs/kernel-review.md`, `docs/kernel-numerics.md`, `docs/kernel-framed.md`, `docs/contracts.md`, `docs/kernel-registration.md`, `docs/cephyr-ffi-module-verification.md`, and `stdlib/on1xfile/README.md` (all relative to `on1x/` where appropriate). Inspect the current completion/capability reports but verify implementation claims against code. Inspect actual native build and runtime SDK paths. Check `cornerstone/LLSynth/README.md`, `docs/interop.md`, and the selected Cephyr embedding/C11 documentation in the pinned LibStone checkout. Paths and symbols in this plan are starting anchors, not substitutes for source inspection.

Extract the ZIP outside the checkout, inspect its contents and run its dry-run installer:

```sh
unzip FFHandler.zip -d /tmp/on1x-ffi-seed
python3 /tmp/on1x-ffi-seed/FFHandler/apply.py /path/to/mulib
```

If anchors match and destinations are absent, apply once on the clean migration branch. If they do not, reconcile the actual source changes manually in reviewed diffs; never bypass anchor errors or overwrite an already migrated implementation. The coordinator owns this seed commit. Record the exact MuLIB and LibStone revisions, dirty state, compiler/tool versions and build configuration in `on1x/hir/FFIHandler/MIGRATION_STATUS.md`.

Run and record the pre-migration baseline in a separate build tree or base worktree before attributing failures to the migration. Discover available CTest cases with `ctest --test-dir BUILD -N`. Save baseline failures and skips; do not erase or silently reclassify them. If the tree is very large, prioritize existing kernel/contract/ABI/C11/HIR/ownership tests first, then the complete release suite.

## 2. Target architecture

```text
                        COMPILER BUILD TIME
FFIHandler.llsynth -- genuine LLSynth --> generated parser
compiler-owned .ffi modules -----------> ffic semantic validator
                                         |
                                         v
                                 immutable typed descriptors

                        PROGRAM COMPILATION
On1x import -> module/member resolution -> foreign descriptor
                                         |
                              typed foreign HIR declaration
                                         |
                canonical type, control, value, ownership verification
                         ^               |              ^
                         |               |              |
                   On1xMETA         FFIHandler        MEMGraph
                   (rewrites)       (boundary)        (loans/places)
                                         |
                              checked C11 marshalling
                                         |
                 canonical MuLIB dependency and native build planner
                         /                            \
                 host C compiler                   Cephyr
                         \                            /
                           verified ABI + linked artifact
                                         |
                         explicit trust/admission policy
```

The DSL defines the foreign module's interface, representation, adaptation operations and foreign semantic assumptions. It does not introduce a second executable IR, interpreter in user programs, or alternate ownership model. The compiler-owned descriptor registry is authoritative for foreign declaration identity; checked HIR remains the semantic authority. `On1xMETA` cannot manufacture admission or foreign proof metadata. MEMGraph's current authored FFI patterns are not an executable proof of arbitrary C behavior; retain canonical verification as mandatory even with optional MEMGraph optimizations disabled.

`MANIFEST.yaml` remains the authoritative source inventory and implementation closure for MuLIB. Keep its real planner and deduplication of implementation switches. Cephyr is an explicit optional provider for C compilation, target ABI verification and diagnostics. Do not make FactIR, Chisel, Satie, ExoML or Exotract compulsory for every ordinary raw scalar call. Reuse genuinely shared declaration extraction where needed without preserving On1x-specific duplicate contract catalogs. Do not remove LibStone components or unrelated MuLIB consumers.

### Canonical layout

```text
on1x/hir/FFIHandler/
    FFIHandler.llsynth
    CMakeLists.txt
    README.md
    INTERFACE.md
    MIGRATION_STATUS.md
    ffi.cpp
    verify.cpp
    lower.cpp
    tools/ffic.py
    generated/                 # genuine reproducible bootstrap, if supported
    tests/
on1x/include/on1x/ffi.hpp       # final public/internal descriptor interface
on1x/stdlib/ffi/mulib/
    clamp.ffi
    media_timebase.ffi
    histprice.ffi
    priceopt.ffi
    ipc_framed.ffi
    ...                        # only actual qualified bindings
```

Names may be adjusted to match existing source conventions, but avoid a second parallel registry. Retire `ffi_scalar.hpp` by folding it into the common schema after consumers migrate; do not keep permanent parallel scalar and general engines. Generated private headers go in the build tree; any tracked bootstrap must come from the genuine pinned generator and be verified against its inputs.

## 3. Design contract (freeze before parallel integration)

### 3.1 DSL

Keep the seed `ffi 1` grammar compatible where practical. Prefer a small finite vocabulary to a general-purpose language. For example, the seed's actual accepted scalar form is:

```text
ffi 1;
foreign mulib.clamp {
    provider mulib "clamp";
    header "mulib-clamp.h";
    fn add_i64(a: i64, b: i64) -> i64 = c::mulib_clamp_add_i64;
    fn sub_i64(a: i64, b: i64) -> i64 = c::mulib_clamp_sub_i64;
    fn i64(value: i64, low: i64, high: i64) -> i64
        = c::mulib_clamp_i64 requires low <= high;
}
```

Extend, in order, fixed-width scalar types and inline functions; exact enum conversion; fieldwise records; immutable slices; initialized out-parameters; checked status-to-sum construction; sealed foreign owners and destructors; bounded call-scoped loans and callback revocation. Do not invent signatures for JSON, Vec, Parsec or Mune: inspect their canonical headers and manuals first. A grammar construct may parse before being executable only if validation explicitly rejects unsupported uses. Every admitted construct must have a real lowering and positive/negative tests.

No arbitrary C fragments, shell, file loading from DSL text, user-selected Lua/native plugins, proof tokens, unrestricted compile flags, or arbitrary evaluation. Bounded input bytes, counts, nesting, arity, operation counts, and diagnostics. Use real LLSynth parse trees, not a second lexer/parser or a regex acceptance fallback. Preserve locations and deterministic descriptor order; validate names, duplicates, referents, provider closure, and type constraints before emitting anything. Schema versions are explicit, unsupported versions fail closed.

### 3.2 Descriptor and marshalling model

Workers 1 and 2 propose and freeze a typed interface with the coordinator before dependent branches implement it. The descriptor must identify module/member, canonical native symbol/header/provider, exact On1x signature, native ABI signature, inline/external placement, bounded marshalling sequence, checks, effect/ownership assumptions, resource identifiers, and supported target/profile. Define a stable resolved foreign identity that cannot be forged through an ordinary source identifier, PPX, or a serialized untrusted HIR object. No name-prefix test alone constitutes authorization.

Use a finite typed marshalling program, not raw C text. Example operation families: evaluate-once operand, check range/relationship, scalar conversion, project field, prepare slice, allocate local out-slot, invoke native symbol, branch on status, read initialized slot, construct result, acquire/transfer/release sealed owner, and revoke callback. The exact enum/schema is the workers' deliverable. Every operation has input/output type, initialization and ownership rules, lowering, and negative cases. Limit the first iteration to operations necessary for real migrated profiles.

### 3.3 Semantics and trust

- Raw imports require lexical `unsafe` at every foreign call, exact ABI, correct representation and well-formed HIR. Unsafe is **not** permission to bypass ABI, initialization, lifetime mechanics, or unsupported operations. Unknown foreign effects are conservative; no purity, reordering, folding, elision or no-retention inferred from silence.
- A checked adapter enforces its actual generated guards and marshalling invariants. It does not prove the foreign implementation respects assumed ownership, error initialization or non-retention.
- Existing safe imports require equivalent independent operator-authorized review and exact subject/artifact checks. Migrate the semantics of that trust gate into a small common FFI policy, rather than retain the old contract framework. No self-authorizing `.ffi` attribute, registry entry, header, proof dump, compile success, or test key. If maintainers later choose a compiler-shipped trusted-library policy, make that an explicit breaking trust-policy decision with documented scope; do not silently downgrade existing safe admission in this task.
- Preserve the old safe/unsafe and static/dynamic spelling distinction, argument evaluation order, traps, success/error semantics, cleanup and callback lifecycle. Keep old source import spelling initially as syntax compatibility (`import kernel`, `unsafe import kernel`, `[dynlib]`); all spellings must reach one implementation. A general `import foreign` alias is optional and lower priority than migration parity.
- Direct C11 is reference. Reject unsupported foreign HIR at MIR/VM/Archon ingress until implemented and verified end to end. No automatic fallback or fictitious support.

### 3.4 Native build and artifacts

Resolve the actual canonical declaration and implementation closure, compare signatures including exact C primitive identities (not just width), validate target ABI/layout/calling convention with generated compile-time checks and independent boundary execution. Use `_Generic`, `_Static_assert`, `sizeof`, `_Alignof`, `offsetof`, and target-specific probes where relevant. Preserve real C header inclusion; do not replace missing standard headers with empty or host-injected substitutes. Cephyr support is scoped to its documented target and C subset. The host compiler remains reference. No silent provider fallback. Each planned implementation switch is defined exactly once, and each companion appears in the correct partition. Hash actual inputs, tool identities, options, target, generated adapters and linked products; publish only after checks succeed. A report/cache hit is not authority to call or load code.

## 4. Parallel orchestration protocol

Create one base seed commit `S0`, then twelve worktrees/branches `ffi/w01` through `ffi/w12`. If branches already exist, preserve them and choose unique names. Use separate `build/ffi-wNN-*` directories or worktree-local equivalents; never share generated output directories. Workers may inspect any file but must not concurrently write outside their assigned ownership. Request shared changes through the file owner. The coordinator may approve a narrow exception and records it in the handoff.

All workers start with inventory, test design, and implementation planning in parallel; dependent implementation waits only for the relevant frozen interface. No worker should independently create a second FFI implementation. In particular, profiles 7–9 add real data and tests and request generic primitives from workers 1–5; they do not add per-kernel branches to the loader or emitter.

Each worker delivers a small sequence of logical commits and a handoff `WORKER-NN.md` (put handoffs in a coordinator-controlled evidence directory, not twelve copies of the same shared status file). Required handoff fields: base commit, branch, scope, files, design decisions, dependencies, tests/commands/versions, exact pass/fail/skip counts, negative controls, residual blockers, and merge instructions. Attach logs or local paths; distinguish tests run from proposed tests. Only the coordinator edits the final shared migration status/release decision.

Use shared design notes/messages for schema changes. A breaking descriptor revision requires re-running all downstream suites. No force-push, no broad formatting changes, no stale branch overwrites, no concurrent edits of `CMakeLists.txt` at the same level. Keep a functioning reference path at every integration gate.

## 5. Exactly twelve workers

### Worker 01 — LLSynth grammar and deterministic generation

**Owns:** `hir/FFIHandler/FFIHandler.llsynth`, `tools/ffic.py` parse-tree extraction and generation orchestration, FFIHandler CMake, grammar fixtures. Own generation CLI and dependency tracking; W02 owns semantic/schema functions (split into a separate module to avoid edits to the same file).

**Tasks:** run genuine pinned LLSynth on v0 and fix parse-tree assumptions; implement bounded grammar for accepted forms in §3; support deterministic, portable source-relative provenance; detect stale/removed source inputs; integrate generated files with `on1x_frontend`; restore a genuine verified bootstrap if required for existing no-generator compiler builds (otherwise fail clearly; document regression, no fake parser). Reuse the On1xMETA snapshot pattern when appropriate. Add parser rejection tests and reproducibility checks in independent checkout paths.

**Done when:** real generator and generated parser pass, no production references to `fixture_parser.py`, grammar validation rejects unsupported syntax, two clean generations yield identical semantic payloads, build dependency changes regenerate correctly. Handoff AST/schema shape to W02; coordinate flags with W06/W10. Do not implement foreign calls here.

### Worker 02 — Generic descriptor schema and semantic compiler

**Owns:** `include/on1x/ffi.hpp` (and transitional `ffi_scalar.hpp`), semantic compiler/emitter module imported by `ffic.py`, `hir/FFIHandler/ffi.cpp` lookup/immutable registry, schema/interface documentation. W05 owns C11 emission implementation, W04 verifier.

**Tasks:** replace arity-only seed representation with typed parameter/result descriptors, normalized symbol identity, nominal resource identity, finite marshalling operations and explicit effect/assumption fields. Enforce uniqueness, closed type shapes, reference ordering, operation/slot budgets, correct status paths, descriptor capabilities, and rejection of unimplemented operations. Emit immutable compiler-owned tables, deterministic manifest, and stable source provenance. Provide lookup by module/member and resolved ID; prohibit accidental collisions between profiles and aliases. No new runtime ABI if avoidable.

**Done when:** W01's genuine parser produces checked data; all C++ consumers can share one schema; malformed/duplicate/forged records rejected; determinism and bounds tested; v0 clamp behavior preserved. Publish frozen `INTERFACE.md` before W03–W09 integration.

### Worker 03 — Loader, namespace resolution and HIR elaboration

**Owns:** `src/loader.cpp`, foreign request/import resolution, required `ast.hpp`/clone/serialization integration, `src/infer.cpp` foreign declaration handling, parser syntax files only if needed. Coordinate any shared AST changes before edits.

**Tasks:** remove kernel-specific member prefix construction and symbol lists as profiles move to descriptors. Resolve registered module/member to a compiler-owned identity; elaborate signatures, sealed nominal identities and explicit foreign calls before inference. Preserve import aliases, source/dependency paths, lexical unsafe permission and direct-call restrictions. Old `kernel` syntax is a compatibility frontend, not a second implementation. Reject unknown exports, cross-module mixups, malformed/stale foreign references and unsupported signatures. Ensure cloning, PPX and HIR wire paths cannot forge privileged identities; reject or version wire fields deliberately.

**Done when:** adding supported functions does not require editing loader/inference; existing clamp source works; scoped alias collision and unsafe negatives, forged-HIR ingress and direct-call tests pass. W04 owns semantic checks, W05 owns emission. Coordinate old `kernel.hpp` helper retirement with W12.

### Worker 04 — Canonical verifier, ownership and MEMGraph

**Owns:** `hir/FFIHandler/verify.cpp`, `src/sema.cpp`, `src/canonical/verify_values.cpp` and closely related canonical verification files, `hir/MEMGraph/` foreign rules and adapter integration; `include/on1x/kernel.hpp` transitional HIR shape checks (coordinate deletion with W12).

**Tasks:** centralize foreign declaration/call checking against resolved descriptors; validate type identity, argument borrow modes, effects, unsafe context, initialization and cleanup. Wire sealed resource/loan obligations into canonical ownership verification, not an optional authored-rule flag. Move ABI/foreign admission-specific rules out of MEMGraph into FFIHandler; retain actual memory and lifetime obligations. Ensure On1xMETA and untrusted HIR cannot upgrade trust or duplicate/remove foreign effects. Preserve conservative MIR ingress rejection (coordinate code ownership for `hir/lower_mir.cpp`). Implement only the resource model needed by W09, reject the rest.

**Done when:** direct verifier negative tests cover wrong type, arity, identity, stale descriptor, forged safe flag, invalid ownership/loan, use-after-move, invalid output read, callback misuse and optional MEMGraph-off parity. No independent ownership engine or arbitrary unchecked metadata.

### Worker 05 — C11 adapter and generic marshalling lowering

**Owns:** `hir/FFIHandler/lower.cpp` (and emission headers), `src/codegen.cpp`, any dedicated C11 bridge emitter. Coordinate with W04 for verifier and W09 for runtime bridge semantics.

**Tasks:** replace per-kernel clamp/Histprice/PriceOpt/IPC lowering branches with generic, typed descriptor operations. Emit C11 exact prototypes, header inclusion and inline forwarding; use already evaluated temporaries in source order, never duplicate expressions. Implement bounded conversions, fieldwise records, pointer+length with explicit validated offsets/extents, out-slot handling, status mapping, and generic resource/callback bridge calls as required. Reuse existing runtime cleanup primitives. Preserve error/trap codes and source locations. Ensure emission checks descriptor identity at ingress. Reference target remains native AMD64 Linux until qualified.

**Done when:** one generic lowering path supports each migrated profile; positive differential tests and evaluation-order/trap/cleanup negatives pass; no module-name switches in emitter for migrated functions; native wrappers compile under strict C11 with GCC/Clang. Unsupported paths fail before artifact publication.

### Worker 06 — Native provider, ABI and MuLIB planning

**Owns:** native compilation orchestration in `tools/on1x-build.py`, `tools/on1x-native-build.py`, the actual `on1x-cephyr-kernel` helper location, reusable portions of `on1x-exotract.py` and MuLIB native source planning interfaces. No changes to global `MANIFEST.yaml` without coordinator review; retain it as canonical inventory. W10 owns project driver and trust policy, W12 deletes obsolete tools only after handoff.

**Tasks:** build adapters/implementation using one shared descriptor-derived closure. Explicit host/Cephyr selection; static and existing `[dynlib]` paths; canonical includes, exact C primitive type and ABI assertions, independent caller/callee probes, dedup switches/companion libraries, validation and atomic publication. Reuse CDecl extraction where required by existing reviewed profiles; do not make a second discovery catalogue. For header-only inline use correct external bridge and no fake implementation macro. Preserve host linking and explicitly reject unsupported Cephyr features/headers. Scope Cephyr analysis outputs as diagnostics, not authorization. Optional mixed-provider builds only when explicit and ABI-probed; no cross-target claims without actual execution.

**Done when:** real clamp ABI test passes host-host, Cephyr-adapter/host-kernel, and independently embedded adapter/kernel on the qualified configuration; wrong prototype and unsupported provider reject; static/dynamic closure tests pass; exact compiler and artifact identities recorded. Additional profiles report real provider eligibility, not silent fallback.

### Worker 07 — Scalar and static-inline profiles

**Owns:** `stdlib/ffi/mulib/clamp.ffi`, `media_timebase.ffi` and other explicitly selected scalar `.ffi` files; scalar/inline examples and profile-specific tests. W01–W05 own shared grammar, descriptor and lowering code.

**Tasks:** audit actual clamp header, migrate all three functions and preserve saturating arithmetic and low/high check. Add a real supported inline scalar, e.g. `mulib_media_timebase_gcd` (actual `uint64_t(uint64_t,uint64_t)` signature), only after U64 lowering is complete. Test signed/unsigned extrema, Boolean and floating types only if actual supported interfaces exist, zero arguments, malformed preconditions and signature mismatch. Keep header-only behavior, no arbitrary external symbol. Prove raw unsafe call works end to end through actual On1x, not just generated C.

**Done when:** old clamp unsafe/reviewed cases retain behavior; selected inline case executes with host reference (Cephyr only if compatible and probed); descriptor-only extension demonstrated without edits to compiler C++ for that function. Add both positive and negative fixtures. Do not advertise catalogue-wide callable coverage.

### Worker 08 — Numerical profiles: Histprice and PriceOpt

**Owns:** `stdlib/ffi/mulib/histprice.ffi`, `priceopt.ffi`, associated numerical examples and profile test fixtures. Coordinate reused records/sums with W02; no domain-specific changes to shared emitter.

**Tasks:** inventory real C declarations, reviewed wire shapes, enum domains, immutable slice bounds and status/error semantics. Migrate both profiles to fieldwise record, bounded slice and out-slot descriptor operations; maintain exact current nominal field ordering and result type identity. Validate mismatched slice lengths, integer conversion and float edge cases, error initialization and failure behavior; preserve external `libm`/dependency plans and current reviewed multi-kernel subject semantics. Do not infer broader admission from previous numerical tests.

**Done when:** previous numerical workflow and multi-kernel suite pass with equivalent explicit review; independent C oracle comparison and error-path negatives; no new C++ per-domain function dispatch. Unsupported Cephyr cases reject explicitly; host success remains available.

### Worker 09 — Foreign resources, IPC, and callback lifetimes

**Owns:** `stdlib/ffi/mulib/ipc_framed.ffi`, resource profile `.ffi` files added here, existing `runtime/on1x_kernel_ipc.*` bridge and resource-specific tests. W04 owns canonical resource verifier; W05 emits generic adapter calls. Preserve existing public runtime SDK ABI unless an explicit migration gate approves changes.

**Tasks:** migrate actual existing one-shot and reusable framed IPC interfaces, sealed owner identities, exactly-once destructor/cleanup, call-scoped exclusive session loans, explicit same-thread callback invocation/revocation, and error status mapping. Preserve no pointer escape, no foreign-thread callback, no unauthorized scheduler interaction. Audit existing JSON/Vec/Parsec/Mune semantic sidecars and classify their needed operations; migrate only interfaces with real complete representation, lifetime and implementation evidence. Add at least one additional bounded resource profile if it can be fully checked and tested without scope explosion; otherwise document exact blockers. Do not invent JSON function signatures or infer no-retention from C `const`.

**Done when:** current IPC tests, owner/loan negative tests, callback lifetime/revoke/wrong-thread/early-exit cases pass (ASan/UBSan where available); no domain-specific verifier/emitter duplication for IPC. General foreign ownership, arbitrary callbacks and native async remain unsupported unless fully implemented and verified. Hand off unadmitted profiles and retained annotations to W12 for archival/conversion, not unsafe promotion.

### Worker 10 — Unified trust policy, project integration and CLI

**Owns:** new common FFI trust-policy module and its tests, project driver `tools/on1x-project.py`, relevant CLI/driver options, `stdlib/on1xfile` project schema/docs, and runtime artifact admission coordination. W06 owns `on1x-build.py` and native provider; request shared edits from W06. W12 owns deletion of old policy/contract files after parity.

**Tasks:** migrate existing independent review semantics to one explicit normalized FFI admission policy: operator key/trust roots, exact reviewed target/signature/contract assumptions/compiler flags/artifact, stale/missing/invalid signature rejection. Test authorities must stay test-only; no self-signing production approvals. Preserve unsafe path without requiring signed review but with ABI/HIR checks. Make On1xfile consume the same native closure and descriptor registry, keeping `kernel` declarations for logistics; profile policy, lock/report provenance, and frozen/sandbox constraints remain validated. Expose one coherent build/inspect CLI, retaining aliases or deliberate migration diagnostics for old command names. If project-local `.ffi` files are supported, stage as declared inputs, default unsafe, enforce normalized paths/limits and forbid execution/authority injection. Do not extend Lua profile return schema gratuitously.

**Done when:** stale/forged/self-authorized imports fail; existing review positive and negative semantics preserved, including reviewed/unsafe alias separation and static/DSO distinction; real On1xfile plan/build/locked/frozen tests pass where supported. Report which old CLI flags are aliases or removed. No permanent contract-specific workflow behind the new CLI.

### Worker 11 — Independent verification, differential and regression gates

**Owns:** consolidated `hir/FFIHandler/tests/integration/` (or existing test conventions), test harnesses and independent oracles, global test registration files only by coordination, verification report draft. No production fixes except by requesting owner changes.

**Tasks:** establish baseline and explicit before/after outcomes; exercise real generated parser, compiler, direct C11, GCC/Clang, optional Cephyr, real static and DSO paths, raw and test-review admissions, real MuLIB headers/implementation TUs. Include malformed DSL, HIR forgery, stale artifact, incorrect ABI, unknown symbol, alias shadow, effect-order, guard/trap, slice bounds, uninitialized output, cleanup/loan/callback tests. Differential oracles must not derive all expected outputs from the implementation being tested. Run sanitizer checks on actual instrumented products; distinguish native products not instrumented. Compare old and new compiler behavior for every migrated legacy profile. Test installation and source/installed usage. Track genuine skips and root cause of failures.

**Done when:** all gates in §7 have recorded reproducible commands/results; regression tests catch deliberately broken implementations (negative controls); no placeholder mocks as production acceptance; unknown results explicitly marked. Notify coordinator of any blocker rather than lowering assertions.

### Worker 12 — Retirement, compatibility audit and documentation

**Owns:** retirement patches, documentation index/readmes, migration notes, old fixtures/scripts after release by their owner, legacy reference inventory. Start by inventorying; **do not delete production files until G4 in §6**.

**Tasks:** map every caller of `kernel.hpp`, `KernelBind`, `contracts-exoml`, `contracts-exotract`, `on1x-bind`, `on1x-contracts.py`, generated discovery catalogs, `contract_admission.hpp`, `contract_cdecl.hpp`, and other legacy artifacts. Search CMake, installation packaging, SDK, Python tools, MCP index, stdlib, examples, CI, docs, tests and other MuLIB consumers. Preserve shared declaration/native planning utilities and useful evidence under sensible common ownership. Convert reusable semantic annotations into `.ffi` where audited; archive historical, unreviewed claims with clear labels when not executable. Once replacement parity is established, remove dead On1x-specific code, duplicate semantic sidecars, separate admission/report workflows and obsolete build switches; migrate tests rather than deleting coverage. Preserve source syntax compatibility and deliberate diagnostic/CLI aliases if needed, not legacy implementation. Update documentation to describe FFIHandler accurately.

**Done when:** clean build/install without retired files, reference audit shows only intentional compatibility/history, no duplicate active dispatch path, migration table of old-to-new files/commands and retained blockers, examples execute, full tests rerun after deletion. If deletion would break an existing qualified behavior, retain only that bounded compatibility implementation and report exact blocker; never declare complete retirement prematurely.

## 6. Merge sequence and dependency gates

| Gate | Integrate | Required evidence before next gate |
|---|---|---|
| G0 — baseline | Coordinator seed + W11 baseline | Clean source inventory, pinned LibStone, historical vs live results separated, baseline test logs |
| G1 — schema | W01 → W02 | Real LLSynth, deterministic validated tables, versioned descriptor interface frozen; W03–W10 may now integrate against it |
| G2 — scalar vertical slice | W03, W04, W05, W06, W07 | Actual On1x clamp calls, explicit unsafe gate, reviewed parity, host ABI, real inline scalar if qualified, no per-domain scalar dispatch |
| G3 — rich profiles | W08, W09, supporting W02–W06 | Numerical and IPC parity, marshalling/ownership negative tests, canonical checks mandatory, no newly unsafe success path |
| G4 — policy/project | W10 + W11 | One common trust/native build policy, exact signed subject checks, project/installation regression, all legacy qualified profiles accounted for |
| G5 — removal | W12 | Remove only released old paths; verify dependency/reference graph; W11 reruns all gates after removal |
| G6 — release candidate | Coordinator + all workers' handoffs | Full suites, reproducible generation, clean install, final manifest of deletions, unsupported scope and explicit blockers |

Parallelism: W01/W02 implement the foundation while W03–W12 inspect code and prepare tests/inventories. W03–W07 can implement against the frozen interface in parallel; W08/W09 author profiles and independent cases concurrently, request schema extensions, and integrate after G2. W10 can design trust/project policy from G0 and integrate after the ABI descriptor interface is stable. W11 tests throughout; W12 inventories throughout. Merge each gate on a coordinator integration branch, resolve conflicts with owners, rebuild from a clean directory at gates, and tag/record the exact tested commits. Do not cherry-pick half of a schema change.

## 7. Mandatory qualification matrix

Use actual CMake option names from the checked-out source. The seed's documented production command is:

```sh
cmake -S on1x -B build/ffi-real \
  -DON1X_LIBSTONE_ROOT="$PWD/on1x/third_party/libstone" \
  -DON1X_REQUIRE_LIBSTONE=ON -DBUILD_TESTING=ON
cmake --build build/ffi-real --parallel 4
ctest --test-dir build/ffi-real -N
ctest --test-dir build/ffi-real -R '^on1x\.ffihandler\.scalar$' --output-on-failure
ctest --test-dir build/ffi-real --output-on-failure
```

Adjust LibStone root to the pinned checkout. Build separate GCC and Clang trees. Configure legacy comparison builds with actual ExoML, ExoML extraction, Exotract and Exotract Lua flags documented in `docs/kernel-bind.md`; configure optional Cephyr and independent checkers using live CMake options. Do not assume a selection with zero tests is success. After retirement, document equivalent new options and commands. Run On1xMETA and MEMGraph generation/snapshot tests and disabled-optional-optimization configurations. Confirm no generator runtime dependency in emitted applications. If an old default/fallback build was supported, preserve it using genuine verified bootstrap or explicitly gate/report the compatibility change.

| Area | Required positive and negative evidence |
|---|---|
| Grammar | Real generated parser accepts actual profiles; invalid syntax, version, unsupported forms, duplicate symbols, depth/size/budget limits reject; regeneration stable |
| HIR | Exact types, nominal IDs, alias scopes, direct calls, unsafe context; forged or malformed HIR/PPX reject; rewrites cannot upgrade trust |
| ABI | Actual header, `_Generic` primitive signature, layout and independent boundary probes; intentionally wrong signature/link target reject; not a proof of C behavior |
| Scalars | Clamp edge cases and reversed interval trap; unsigned static-inline oracle, no fake external symbol; real On1x execution |
| Records/slices/status | Field order, extent, offsets, checked conversion, error branch, no uninitialized output reads, left-to-right once-only argument evaluation |
| Ownership | Exactly-once cleanup; failure/early exit paths; stale loans, double drop, wrong owner, callback revocation and thread negatives |
| Trust | Valid test signature only under explicit test key; stale source, wrong key, tampered adapter/artifact, flags, target and linkage mismatch all reject; no safe-by-metadata |
| Native planner | Canonical closure, one implementation TU, external companions, static/DSO split, no fallback, no partial publication on error |
| Cephyr | Actual supported AMD64 profiles; existing clamp boundary probe (discover exact test name; previously `on1x.cephyr.clamp_abi`), wrong prototype; explicit unsupported-case failure |
| Backend | C11 reference output/traps/cleanup; MIR/VM/Archon explicit rejection of unsupported foreign operations |
| Project/installation | Real On1xfile plan/build, lock drift, frozen/no-network, sandbox constraints, staged input validation, installed compiler usage |
| Retirement | Fresh configure/build/install after deleting old files, all equivalent regressions still present, audit references and flags |

Run ASan/UBSan with actual applicable C/runtime products; report leak detection configuration and limits. Use independent mixed compiler caller/callee probes, not just the same compiler on both sides. Use separate target execution if making any cross-target claim. A successful compile, a solver result about one adapter arithmetic expression, or a static analysis warning is not general foreign-function safety. Benchmark compile/runtime overhead only if controlled same-input baseline and correctness gates pass; report measurements, not assumed speedup.

## 8. Retirement criteria and non-goals

The goal is to eliminate **the standalone On1x contract architecture and KernelBind implementation**, not rename them while preserving all duplicate machinery. The final compiler must not contain individual MuLIB domain-specific type/marshalling dispatch for migrated functions. The following are candidates for removal or factoring, subject to the W12 dependency audit:

- old scalar/numerical/IPC symbol lists and specialized HIR shape helpers;
- old On1x-only contract inventories, semantic JSON sidecars and duplicated declaration/binding catalogs;
- standalone `on1x-bind` / `on1x-contracts.py` entry points, specialized build/admission code and obsolete CMake options;
- old report formats and legacy documentation, except compatibility diagnostics, historical evidence, or shared non-On1x tools still used elsewhere.

Retain actual native declaration checks, common MuLIB MANIFEST-based planning, canonical type/ownership verification, native builder and runtime, review verification as an explicit policy for existing safe interfaces, and all useful regression tests. Do not delete global ExoML/Exotract/Chisel/Satie or vendor LibStone code. Do not migrate all thousands of discovered headers into callable interfaces automatically. Discovery is not ABI qualification or behavioral evidence.

General safe C FFI, arbitrary pointer/varargs/callback semantics, lifetime inference from C, all-platform aggregate ABI, new MIR FFI, cross-language LTO, general dynamic JIT loading, and complete networking admission are **not required** for this migration. They must reject or stay explicitly out of scope. Additional qualified profiles are welcome only after core parity; no fabricated declarations or admission claims.

## 9. Definition of done and final report

The supervising agent must deliver real code on the integration branch, not just twelve worker reports. Final `MIGRATION_STATUS.md` must contain:

1. Exact tested MuLIB/LibStone commits, toolchain versions and configuration, whether the tree was clean, actual worker completion, and merge commit order.
2. Old-to-new map of APIs, CLI commands, files, deleted subsystems, and retained shared infrastructure; distinguish active compatibility from history.
3. List of actual callable profiles/exports, target and compiler providers, trust modes, supported representations, and explicit rejections.
4. Test commands/log paths with counts, failed/skipped cases and reasons, real LLSynth evidence, independent ABI and negative-control outcomes, sanitizer and optional-provider scope.
5. Remaining blockers classified as **verified**, **executable**, **partial**, or **absent**, with source/test scope. Do not reuse these labels to imply production safety or certification.
6. A demonstration of adding a supported function by changing only its `.ffi` definition and tests (no compiler source edit), followed by a real successful On1x build and execution.
7. A release recommendation: complete retirement only when every old qualified behavior is preserved or an explicitly authorized breaking change is documented; otherwise partial migration and exactly what remains. No invented timings, test runs, approval or production safety guarantee.

Success is **a smaller, coherent FFI system with real executable bindings**. The essential metric is that foreign declarations become data, boundary semantics become generic checked compiler operations, and unnecessary legacy frameworks actually disappear without weakening the language's guarantees.
