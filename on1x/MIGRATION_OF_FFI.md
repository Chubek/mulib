# MIGRATION_OF_FFI

**Agent implementation directive — revision 2: Cephyr semantic projection**  
**Repositories:** `Chubek/mulib` (On1x), `Chubek/libstone` (Cephyr and LLSynth)  
**Parallelism:** exactly 12 implementation workers plus a supervising coordinator.  
**Supersedes:** the previous 12-worker FFI migration directive and its manual-binding assumptions.  
**Objective:** automatically generate the standard MuLIB foreign-interface library in `on1x/stdffi/mulib` from Cephyr's existing resolved declaration projection; compile it with LLSynth-based FFIHandler; replace KernelBind and On1x-specific standalone contracts with one generic, checked FFI implementation.

## 0. Orders to the supervising agent

Implement, integrate, test and remove superseded machinery. Do not merely write another plan or produce twelve design reports. Launch the twelve workers in §6 using independent branches/worktrees and separate build directories. The coordinator owns integration and release decisions, not a thirteenth implementation package. If the environment cannot launch twelve real workers, explicitly report that limitation and execute the same work packages in dependency order. Never claim nonexistent workers or unexecuted tests.

Use the live checkout as authority. Read applicable `AGENTS.md` files, inspect uncommitted changes and pins before writing, and preserve unrelated work. Do not push, force-push, change the LibStone pin, rewrite public MuLIB headers, or grant new trust authority without authorization. A newer upstream commit is evidence, not permission to change the dependency silently. Use an explicitly selected LibStone checkout for qualification if the currently pinned dependency lacks the interface, and record the integration blocker until the pin is authorized.

The previous `FFHandler.zip` is a seed/reference, not a dependency to reinstall blindly. On1x already contains FFIHandler v0 in the inspected GitHub tree. Do not rerun the archive installer over an existing implementation. Reconcile the actual tree, including any newer worker changes. No fabricated parsers, handcrafted generated-looking binding files, silent fallback to Clang/ExoML, permissive stubs, or hidden second FFI implementation. Keep existing verified call behavior until its replacement passes the corresponding gate. Partial migration must be reported as partial, never disguised as completion.

## 1. New upstream evidence: consume what Cephyr already implements

The inspected LibStone `master` head was `e10ac69eed7baf91b06d15c4e0d58360097cb5c4`. The following are Git blob IDs (not SHA-256 digests or execution evidence). Pin the actual selected checkout and calculate local SHA-256 inputs in the reproducibility report. Do not assume that the inspected head is already On1x's dependency.

| Source at inspected LibStone revision | Git blob |
|---|---|
| [C11Interface.md](https://github.com/Chubek/libstone/blob/e10ac69eed7baf91b06d15c4e0d58360097cb5c4/cornerstone/Cephyr/docs/C11Interface.md) | `ec6fbb0fdea35079221044c9067466635d265955` |
| [interface.cpp](https://github.com/Chubek/libstone/blob/e10ac69eed7baf91b06d15c4e0d58360097cb5c4/cornerstone/Cephyr/src/interface.cpp) | `01b8dc40f79e401f86035c8358c2584ac5900343` |
| [interface.hpp](https://github.com/Chubek/libstone/blob/e10ac69eed7baf91b06d15c4e0d58360097cb5c4/cornerstone/Cephyr/src/interface.hpp) | `5fe2ba5b367e2e299fe66b25c5efad9da0d9733d` |
| [test_c11_interface.py](https://github.com/Chubek/libstone/blob/e10ac69eed7baf91b06d15c4e0d58360097cb5c4/cornerstone/Cephyr/tests/test_c11_interface.py) | `4533b99adbf46609bc2baa49b4a74936f8c8fbe6` |
| [EMBEDDING.md](https://github.com/Chubek/libstone/blob/e10ac69eed7baf91b06d15c4e0d58360097cb5c4/cornerstone/Cephyr/EMBEDDING.md) | `979ef3703c2cb32c9ab063919e1606a8a7fa0f7a` |
| [Cephyr.h](https://github.com/Chubek/libstone/blob/e10ac69eed7baf91b06d15c4e0d58360097cb5c4/cornerstone/Cephyr/include/cephyr/Cephyr.h) | `01c49f194e39e98b7cbdf1d4ba31975712a4ddc0` |
| [Cephyr.hpp](https://github.com/Chubek/libstone/blob/e10ac69eed7baf91b06d15c4e0d58360097cb5c4/cornerstone/Cephyr/include/cephyr/Cephyr.hpp) | `26350fd74480257783ef293068332ed179497ba3` |

These files were inspected, not built or executed during preparation of this directive. W02 must qualify the actual tool. The earlier proposal to implement `--emit-interface=sexp` from scratch is obsolete. Do not duplicate Cephyr's semantic resolver in Python, On1x, LLSynth or another library. No new CDecl framework.

### 1.1 Real supported entry points

```sh
cephyr --target=x86_64-linux-gnu --emit-interface=sexp example.c
cephyr --target=aarch64-linux-gnu --emit-interface=sexp example.c -o example.interface
cephyr --target=x86_64-linux-gnu --emit-interface=sexp -x c - < example.c
```

Also available: `cephyr_emit_interface(&request, &result)`, `cephyr::emit_interface(args, source)`, Python `Compiler.emit_interface(args, source)` and `cephyr.emit_interface(...)`. For SDK entry points omit the stage option from the request arguments; inspect status/output/diagnostics and destroy native results. Start with a subprocess using explicit argv, no shell. Do not require embedding to complete the migration; SDK parity is useful validation.

`--emit-interface=sexp` is an exclusive stage: do not combine with `-E`, `-fsyntax-only`, `--emit-ast`, `--emit-mir`, `-S` or `-c`. Accepts one raw C TU, preprocessed C, or validated C11Syntax document. The direct raw-C route is preferred for production generation; no mandatory raw AST round trip. `--emit-interface=json` is not supported by this inspected implementation. C11Syntax JSON/YAML/sexp and C11Interface sexp are distinct formats. Do not confuse their envelopes or readers.

### 1.2 Actual wire format and semantics

The projection begins `(interface 1 (target "x86_64-linux-gnu") (data-model LP64) (abi "SysVAMD64") ...)`. It includes `typedef`, `object`, `function`, and `tag` forms. Exact implementation shapes are documented and exercised by the upstream tests. For illustration, this is a scalar function shape consistent with the serializer, not a captured output from a locally executed Cephyr:

```lisp
(interface 1
  (target "x86_64-linux-gnu")
  (data-model LP64)
  (abi "SysVAMD64")
  (function "example_add"
    (linkage external) (storage none) (definition declaration)
    (calling-convention sysv) (parameter-names "a" "b")
    (specifiers)
    (type (function-type (prototype true) (variadic false)
      (result (integer long (rank 4) (bits 64) (signed true) (size 8) (align 8)))
      (parameters
        (integer long (rank 4) (bits 64) (signed true) (size 8) (align 8))
        (integer long (rank 4) (bits 64) (signed true) (size 8) (align 8))))))
```

- Integer primitives include C spelling identity, rank, width, sign, size and alignment. Do not conflate `long` with `long long`, even on LP64. Preserve plain `char`, `_Bool` and floating representation identity; obtain exact atom spellings from serializer and real fixture output. Preserve native and On1x surface types separately.
- Function types distinguish `f()` from `f(void)` and expose prototype, variadic status, adjusted parameter types, return type and calling convention. Array/function parameters are adjusted to pointers; top-level parameter qualifiers are removed; pointed-to qualifiers remain. An array parameter's `static` minimum bound is **not** preserved by the signature projection.
- Compatible redeclarations are merged; storage specifiers are a sequence in source order, not a single enum. Parameter names prefer the definition, then a named declaration; they can be `none`. Names are not stable semantic identities; use parameter positions for overrides and fingerprints, preserve names for display and validate collisions.
- Effective linkage is separate from written storage. `definition` can be `declaration`, `tentative`, `defined`, or `inline-only`; `specifiers` may include `inline` and `noreturn`. `static inline` is not an exported object symbol. External `inline-only` is not automatically equivalent to an external definition or an internal static inline.
- Tags have document-local IDs (including zero), kind, possibly anonymous name (`none`), completeness, layouts and fields or enumerators. Tag references can be recursive. Never identify tags by name alone or persist local IDs as cross-document identity. Distinct prototype-scope tags with identical spelling may be distinct. Preserve byte offsets, bit-field width/shift/container offset, and flexible-array flags. Preserve `unknown` rather than zero. Treat unions, bit-fields, anonymous/recursive by-value shapes and incomplete types as unsupported for callable lowering until specifically qualified.
- Built-in projections: `x86_64-linux-gnu` / `SysVAMD64` / `sysv`; `aarch64-linux-gnu` / `AAPCS64` / `aapcs64`; `riscv64-linux-gnu` / `LP64` / `lp64` (RV64IM). All LP64; target-specific layouts still differ. No SDK-only target ABI support. Projected calling convention is not a register assignment or executable ABI witness.

### 1.3 Critical gaps that the consumer must respect

1. Includes contribute declarations. Interface v1 has no per-declaration source file/line or public-export marker, and is not a symbol table. Do not automatically export every `function` form or infer origin from prefix alone. Require explicit canonical module/export selection (§3.2). Keep raw AST/source-map inspection separate and optional for attribution; never use it to independently resolve types.
2. Projection resolves file-scope declarations, not native function-body lowering or all implementation constraints. The upstream tests deliberately demonstrate a body with VLA that projects but fails full checking. A successful projection is discovery/type evidence only. Native compilation, link and boundary probes remain mandatory for callable admission.
3. C declarations do not establish ownership, nullability, pointer retention, error behavior, effects, cleanup, scheduler/authority, thread-safety or output initialization. Cephyr AST/analysis may assist an audit, not silently grant safety.
4. Interface output does not carry full input/tool/preprocessor provenance, target flags or implementation artifact digests. The generation driver must record these separately. Hashes identify bytes; they are not approval.

## 2. Baseline, target architecture and layout

Before changes, record `git status --short`, `git rev-parse HEAD`, `git submodule status --recursive`, actual tool versions, compiler selections and build options in `on1x/hir/FFIHandler/MIGRATION_STATUS.md`. Discover CTest cases with `ctest --test-dir BUILD -N`. Run baseline kernel, contract, ABI, C11, ownership/HIR, project and full tests in separate build trees; capture failures and skips rather than attributing all existing failures to migration. Read current `on1x/hir/README.md`, `hir/FFIHandler/{README,INTERFACE}.md`, `hir/processor/{INTERFACE.md,CMakeLists.txt}`, `hir/MEMGraph/ADAPTER.md`, `docs/{kernel-bind,kernel-review,kernel-numerics,kernel-framed,contracts,kernel-registration,cephyr-ffi-module-verification}.md`, `stdlib/on1xfile/README.md`, actual planner/runtime code and current completion reports. Also inspect LLSynth README/interop and Cephyr C11/interface/embedding docs at selected pin.

At previous inspection, FFIHandler had a scalar descriptor implementation in `hir/FFIHandler`, `include/on1x/ffi_scalar.hpp`, and `tools/ffic.py`; CMake referenced `on1x/stdlib/ffi/mulib/clamp.ffi` (the file itself was not retrievable in that view). Do not assume this layout is unchanged. Existing `ffic.py` restricted parameters/results to i64, packages to 64 files and 256 exports, parameters to 16, with canonical prefix checks. Expand through bounded per-module units, not unbounded global tables. Existing scalar path still relies on old admission/build policy. MuLIB previously documented 2,053 registered public headers; re-enumerate current `MANIFEST.yaml`, do not hardcode a total. Registration is not callable coverage.

```text
MAINTAINER / CI
  MANIFEST.yaml + canonical header closure + explicit target/environment
      -> Cephyr --emit-interface=sexp (resolved declarations)
      -> bounded S-expression decoder + typed projection consumer
      -> canonical export selection + native/surface representation mapping
      -> merge audited FFIHandler semantic overrides
      -> deterministic on1x/stdffi/mulib/*.ffi + one inventory/provenance index

COMPILER BUILD
  FFIHandler.llsynth -> genuine LLSynth parser
  generated .ffi + ffic validator -> immutable typed descriptor registry

APPLICATION BUILD
  import -> descriptor resolution -> canonical checked HIR
      -> canonical verifier + MEMGraph ownership/loans + On1xMETA
      -> generic C11 marshalling -> native planner (MANIFEST)
      -> explicit host or Cephyr compile/link -> ABI/artifact checks
      -> explicit trust policy -> publication

APPLICATION RUNTIME
  native calls and existing On1x runtime; no generator/reader/extractor runtime
```

Canonical checked HIR remains semantic authority; FFIHandler defines foreign boundary operations, not another executable IR or ownership system. `MANIFEST.yaml` remains implementation inventory/closure authority. Cephyr is mandatory for regeneration/qualification of generated bindings; application compilation may use an explicitly selected supported host or Cephyr provider. Do not require Cephyr to parse the full library on every application build. Keep direct C11 as reference; unsupported MIR/VM/Archon foreign operations fail before codegen, with no automatic backend fallback.

### 2.1 Canonical layout (new paths are implementation targets)

```text
on1x/
  stdffi/
    README.md
    mulib/
      clamp.ffi
      media-timebase.ffi
      histprice.ffi
      priceopt.ffi
      ...                         # deterministic file per registered module
      index.json                  # generated coverage/provenance, not authority
    overrides/
      mulib/
        clamp.ffi                 # authored semantic deltas only
        ...
  hir/FFIHandler/
    FFIHandler.llsynth
    CMakeLists.txt
    README.md
    INTERFACE.md
    MIGRATION_STATUS.md
    ffi.cpp
    verify.cpp
    lower.cpp
    tools/
      cephyr_interface.py         # bounded reader, schema and graph validation
      project_interface.py       # Cephyr invocation/provenance (W02)
      ffgen.py                    # generation/merge orchestration (W03)
      ffi_types.py                # representation mapping (W03)
      ffic.py                     # real LLSynth parser and descriptor compiler
    tests/
  include/on1x/ffi.hpp             # common descriptor API, replaces ffi_scalar.hpp
```

Use manifest-key filenames (e.g. `media-timebase.ffi`) and existing canonical On1x alias normalization (e.g. `mulib.media_timebase`) only after checking actual manifest conventions; collisions reject. Do not preserve `on1x/stdlib/ffi/mulib` as a second registry. Move existing declarations, references, packaging, examples and tests to `on1x/stdffi/mulib`; migrate hand-written semantic content to `stdffi/overrides/mulib`. No hand edits to generated files. Generated compiler headers in build tree; checked-in genuine bootstrap only if needed for compatibility and mechanically verified. Do not check in raw .interface/.sexp dumps for all kernels; keep compact golden fixtures and generated standard interfaces. One stable index, no per-kernel contract-report hierarchy.

## 3. Generation and provenance contract (freeze at G1)

### 3.1 Bounded projection reader

Parse actual interface v1 S-expressions: single root, integers, atoms, JSON-escaped quoted strings, balanced lists, valid UTF-8. Distinguish quoted strings from atoms (including `none` and `unknown`). No Lisp reader macros, symbol evaluation, arbitrary code, paths or commands. Implement byte/token/node/depth/string/count/numeric limits before allocations or recursive expansion, bounded diagnostics and timeout/output caps for subprocesses. Reject unknown versions, mismatched target/data-model/ABI, duplicate singleton clauses, invalid enums/types/integers, dangling/duplicate tags, incompatible shapes, duplicate declarations, malformed lists, trailing forms and invalid escapes. Unknown schema fields must fail closed or be an explicit versioned discovery-only extension, not silently trusted. Validate graph with iterative traversal and memoization; recursive pointer graphs are legal, impossible by-value cycles reject. Preserve identity and exact native primitive fields; no type resolution from C spellings. Reuse the public format and fixtures, not private Cephyr C++ internals. The reader is a consumer of a structured interchange, not another C parser.

### 3.2 Canonical inventory and export ownership

For each registered module, create a declaration-only TU that includes its canonical public header, not a synthetic guessed prototype or rewritten source. Resolve include roots, feature macros and target environment through the real MuLIB manifest/planner. Do not define implementation switches for extraction unless a documented profile explicitly needs them; implementation compilation is a separate phase. Header-only static-inline functions already have bodies. No fake system headers or copied host includes to cover unsupported Cephyr syntax. Run each module in a controlled working directory; use explicit trusted binary, target, sysroot/config/header settings and bounded argv. Disable ambient config/hooks unless explicitly selected and recorded (inspect Cephyr's actual supported switches and env variables).

Projection includes system and dependency functions. Define a deterministic, audited export-selection rule: use canonical manifest module, canonical naming convention, and verifiable origin evidence or explicit symbol selection for exceptions/ambiguity. Interface v1 itself lacks source origin; do not label prefix matching as proof. A practical default is select names under the module's reserved prefix only after checking the actual transitive include closure for competing declarations; use canonical source attribution from C11Syntax source maps if sufficient, or a small explicit allowlist/denylist in the same authored FFI override language when ambiguous. If ownership cannot be established, record `ambiguous_export` and keep discovery-only. Never automatically include `printf`, `malloc`, imported dependency symbols, internal helpers or data objects just because they appear in the TU. Retain per-module aliases without cross-module leakage. Explicitly account for CDD/multi-header modules, header-only modules, data-only modules, and module-specific prefix conventions. A selected external declaration still needs an actual linked symbol; static inline needs a verified local forwarding wrapper; an external inline-only function needs a separately qualified implementation/linkage recipe or remains unsupported. Macro-only APIs are not C declarations and stay unsupported unless an explicit bounded bridge exists.

### 3.3 Native and surface types

Store resolved native C types and On1x-facing types independently. Begin with exact i64, then U8/U16/U32/U64, f32/f64, Bool and void if supported. A resolved C `int` of 32 bits cannot be silently treated as an i64 ABI slot. If exposing it as i64, check narrowing, use exact native prototype, extend the result correctly. `size_t`, `ptrdiff_t`, plain char and long double require exact target mapping; no unsupported silent widening. Scalar mapping is conditional on actual adapter checks. Enums retain underlying primitive and value domains; sums require explicit conversion rules including unknown discriminants. Record adapters should marshal fields into native C types and use exact layout assertions, not assume identical On1x object layout. Pointer/array/function-pointer projections are descriptive until the appropriate loan/resource/length/callback rules and native bridge are implemented. Reject unprototyped, variadic, unsupported qualifiers/volatile/atomic, noreturn without verified control effects, unsupported aggregates and unsafe lifetime assumptions. Distinguish unsupported discovery from invalid input.

### 3.4 Generated `.ffi` and small semantic deltas

Maintain one FFIHandler grammar. Extend it for generated declaration-only and unsupported entries, versioned native types, explicit profile eligibility, and an `extend`/override form (exact syntax frozen by W03/W05). Do not introduce a second hand-authored JSON contract language. The seed `ffi 1` syntax should remain readable where practical. Proposed output for clamp (signatures extracted, precondition from audited override):

```text
# GENERATED; edit canonical header or semantic override, not this file.
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

The full new schema must also carry native type identity/target and provenance binding (in descriptor or associated generated index), so this abbreviated seed syntax is not enough by itself for expanded ABIs. The authored override should contain only the low/high check, bound to the exact extracted symbol/signature. For anonymous or conflicting parameter names use deterministic `argN`; overrides bind by position plus exact signature; parameter renames must not redirect a guard. Preserve original names for diagnostics when available. Audit old semantic sidecars: translate only necessary assumptions and adaptations, not every repetitive annotation. For JSON/Vec/Parsec/Mune, preserve unresolved obligations even if calls are unsupported. Override must not alter native signature or invent C symbol, grant safety, inject C/shell/include paths or mark unavailable lowering executable. Fail stale/ambiguous overrides; do not silently discard them. `requires` is a runtime caller check, not a claim about the C body. Status/ownership facts remain assumptions subject to explicit admission.

### 3.5 Reproducibility, incremental regeneration, package size

Proposed maintainer CLI (implement and document these flags, names may be aligned with existing conventions):

```sh
python3 on1x/hir/FFIHandler/tools/ffgen.py --mulib-root . \
  --cephyr /abs/path/to/cephyr --target x86_64-linux-gnu \
  --output on1x/stdffi/mulib --all
# One module; same explicit configuration
python3 on1x/hir/FFIHandler/tools/ffgen.py --mulib-root . \
  --cephyr /abs/path/to/cephyr --target x86_64-linux-gnu \
  --output on1x/stdffi/mulib --kernel clamp
# CI: read-only freshness and deterministic regeneration check
python3 on1x/hir/FFIHandler/tools/ffgen.py --mulib-root . \
  --cephyr /abs/path/to/cephyr --target x86_64-linux-gnu \
  --output on1x/stdffi/mulib --all --check
```

All inventory entries must end in an explicit outcome: parsed/extracted/representable/discovery-only/unsupported/failed with stable reason codes. Generate a valid non-callable module document for failures or unsupported modules with no fake declarations; include exact reason and source identity in the index. Distinguish unattempted from failed. Do not count skipped headers as successfully projected. Generate whole manifest inventory with bounded parallel jobs, deterministic ordering, per-module time/memory/output budgets. Compiler build should not load all interfaces into one unbounded `ffic.py` invocation. Use deterministic per-module/chunk compilation and an indexed registry with explicit limits. Discovery-only modules need not inflate executable descriptors.

Record actual Cephyr executable digest/version, selected LibStone revision, generation/grammar/override hashes, target/ABI, effective include dirs/sysroot/config/defines/preprocessing options, manifest identity, canonical transitive header content hashes, projection digest, normalized signature digest and emitted `.ffi` digest. Record environment inputs which affect preprocessing. Use a real dependency trace or conservative hashed complete trusted include-root inventory when exact trace is unavailable; never claim a direct-header hash covers transitive dependencies. Check input stability during generation. Cache by actual input identities; a cache hit is never admission. Canonicalize output ordering, paths relative to declared roots, and content formatting; no timestamps or absolute temp paths in committed output. Where tag IDs or parameter names vary, define deterministic canonicalization without merging distinct types. Separate target-qualified descriptions/metadata, never overwrite AMD64 inventory with cross-target output accidentally; reject incompatible target at import. Commit default AMD64 catalogue; other targets use explicit namespaces/output paths or target-qualified entries.

Build staged output in a private directory; validate all documents through genuine LLSynth before publishing. Single-module updates must atomically replace their file/index consistently (use versioned package or journal/manifest commit marker); full refresh must not leave mixed generations or stale removed modules; preserve prior good output on unexpected failure. Unsupported outcomes may publish only when complete, explicit and deterministic. `--check` must not mutate committed files and must fail on missing, changed or unexpected stale generated files, target drift or stale overrides. CI should include clean checkouts with different absolute paths. No generator or extractor at application runtime.

## 4. Generic compiler and native boundary contract

### 4.1 Descriptor registry and HIR

Freeze an immutable schema with canonical module/member/provider/header, native symbol, linkage/placement, exact native ABI signature, exact On1x signature, supported target, stable resolved ID, bounded marshalling plan, effects/assumptions, resource IDs, and explicit admission state managed by compiler/driver (not DSL). Foreign ID must not be forgeable through source, PPX, HIR wire input or optimizer rewrites. Revalidate descriptors at canonical ingress and C11 emission. Retire arity-only scalar model after migration; avoid two permanent engines. No domain-specific name switches in loader/infer/emitter/verifier for migrated functions. Preserve existing `import kernel`, `unsafe import kernel`, `[dynlib]`, aliases and direct-call restrictions as syntax compatibility into one engine. Optional general `import foreign` alias only after parity.

A finite typed marshalling program may include evaluate-once, range/relationship checks, scalar conversion, project field, prepare bounded slice, allocate local out-slot, call, branch on status, read initialized output, construct sum, acquire/transfer/drop sealed owner and callback revocation. Every operation needs verifier semantics, initialization and ownership/effect constraints, actual lowering and negative tests. No arbitrary C snippets, embedded actions or general-purpose foreign interpreter. Preserve left-to-right evaluation exactly once, trap codes, error/success branch semantics, source locations, cleanup/defer order, and no elision/reordering of unknown effects. Use existing ownership verifier and MEMGraph for loans, places, initialization, aliasing and lifetime constraints; move foreign ABI/admission specifics into FFIHandler. MEMGraph authored rules are not proof of arbitrary C behavior. Optional optimizations cannot disable canonical admission or manufacture proof metadata.

### 4.2 Native planner, ABI and runtime

Retain canonical MANIFEST closure, native companion selection, external libraries, exact compiler options, one implementation TU per partition and deduplicated macros, header-only handling and static/DSO semantics. Generate real C11 adapters including canonical headers; use `_Generic`/`_Static_assert`, `sizeof`, `_Alignof`, `offsetof` where applicable and independent caller/callee boundary probes against actual linked objects. Compare native primitive identity (not merely size/signedness). The C compiler checks the projected declarations under the actual compilation environment. Require link/export presence for external functions; static inline wrappers must compile in the correct TU. Probe actual DSO if selected; no claims based only on archive symbol names. Explicit host/Cephyr selection, no silent fallback, explicit unsupported provider diagnostics. No target claim without execution on target or a documented equivalent emulator environment. Preserve platform/scheduler/runtime restrictions of current profiles. Cephyr projection, optional FactIR/Chisel/Satie analyses, and native compile success do not grant safe admission.

### 4.3 Trust policy (replace architecture, not safeguards)

- Automatically extracted declarations are discovery data. Representable means a supported mapping exists. Callable-unsafe requires valid native build, ABI checks and explicit lexical `unsafe` at the call site; even unsafe cannot bypass well-formedness, ABI, lifetime mechanics, initialization or unsupported operations. Unknown effects remain conservative. Do not promote all scalar functions merely because their signature is representable; verify actual eligibility.
- Checked adapters enforce the declared runtime checks but cannot prove foreign retention, destructor, output initialization or effects. A trusted safe interface requires explicit authority and semantic assumptions bound to the exact target, headers/implementation closure, native signature, overrides, adapter, flags/compiler and artifact. Preserve existing independent operator-selected Ed25519 review semantics for previously reviewed imports. Migrate this into one compact FFI policy instead of retaining separate contracts. A generated `.ffi`, digest, self-reported `safe`, test key, proof dump or metadata report never grants authority. Do not silently authorize safe calls or reinterpret old review subjects as approval of new binaries. If maintainer wants a different shipped-library trust model, that is a separate explicit decision.
- Preserve old source/static/dynamic spelling and ABI review gates until replacement parity. Build-driver plan/report format may change; equivalent subject verification, replay/tamper resistance, and artifact checks must not. Do not use old approval for a changed subject. Keep no production signing key in repository. Test keys limited to test process. Ensure install/SDK cannot bypass policy.

## 5. Parallel execution protocol

Create a coordinator base commit `S0`, twelve branches/worktrees `ffi/w01`…`ffi/w12` (use unique names if occupied), separate work/build directories, and an integration branch. All workers begin with inventory/test design concurrently; dependent implementations wait for frozen schema, not for the whole preceding worker to finish. Workers inspect any file but write only owned areas; cross-cutting changes go through owners. Coordinator merges small commits at gates and resolves conflicts with owners. Keep a runnable reference path until replacement. No shared generated output or concurrent edits to CMake/registry/index. W04 alone publishes generated inventory; other workers submit override changes to their assigned module files and request regeneration. W05 owns FFIHandler CMake and common schema; W10 owns project/native-driver wiring. W11 independently owns qualification harness; do not let implementation workers certify themselves.

Each worker submits `WORKER-NN.md` in coordinator-controlled evidence directory: base/branch/commit IDs, actual files, design/schema version, dependencies, exact test commands/tool versions/pass/fail/skip counts, logs, negative controls, limitations, and merge order. Report failed attempts. No fabricated tests, coverage or timings. Coordinator alone updates shared final migration status and release decision. If an interface changes, invalidate/re-run all affected gates.

## 6. Exactly twelve workers

### W01 — C11Interface v1 bounded decoder and typed model

**Owns:** `hir/FFIHandler/tools/cephyr_interface.py`, reader model/schema fixtures and tests. **Depends on:** real public interface docs/serializer; coordinate with W02/W03.

Implement §3.1 using the actual interface S-expression, not C11Syntax `(1 (object ...))` and not hypothetical format from the old migration. Decode function/object/typedef/tag, qualifiers, primitive/layout identity, composite types and recursive IDs; strict version/target/ABI validation, bounded graphs, unknown markers. Preserve quoted-vs-atom semantics. Adapt upstream fixture patterns, add adversarial cases. No Cephyr subprocess or C type resolver in this module. No serialization-dependent whitespace matching.

**Done:** real W02 output consumed for all three built-in target projections; direct, preprocessed and syntax-input equivalents normalize identically; malformed, duplicate, cyclic-by-value, deep, oversized, unknown-target and dangling-tag cases reject. Publish typed consumer API and test fixtures before G1.

### W02 — Qualify existing Cephyr projection and generation provider

**Owns:** `tools/project_interface.py` within FFIHandler, Cephyr invocation/SDK parity and provenance tests; upstream issues/reproducers only unless explicitly authorized to patch LibStone. **Depends on:** actual selected LibStone checkout.

Build Cephyr with actual documented CMake options, run upstream `tests/test_c11_interface.py` against real executable and relevant embedding/parser tests, inspect pin, compare source/preprocessed/C11Syntax projection routes, verify target identity and primitive/layout differences. Implement controlled per-TU CLI invocation, stage exclusivity, capture diagnostics, caps/timeouts, deterministic environment and dependency/config tracing. Use real API rather than write new semantic projection. Test missing/old Cephyr, unsupported syntax, no fallback, malformed/truncated output, declaration-valid/body-invalid example, and actual clamp header. Identify lack of origin data and provide attribution evidence to W03 (C11Syntax source map only when adequate; no new C resolver). If pin lacks feature, report blocker and qualify explicit selected checkout without silently changing pin. Propose minimal upstream patch only for demonstrated blocker; no speculative redesign.

**Done:** reproducible real projections and upstream tests with recorded commit, actual compiler path, status and hashes; W01 can consume them; no claims of native execution from projection. Host/native execution is W10/W11.

### W03 — Automatic `.ffi` generation and representation policy

**Owns:** FFIHandler `tools/ffgen.py`, `ffi_types.py`, generator/override merge logic and generation unit tests. **Depends on:** W01 reader, W02 provider, W05 grammar/schema; W04 inventory contract.

Implement §3.2–3.5: canonical naming, export ownership, native/surface mapping, generated unsupported entries, deterministic `.ffi` rendering, strict override match by native signature and parameter positions, `--kernel`, `--all`, `--check` orchestration. Use actual manifest/planner interface supplied by W04/W10. No C text parsing or C type reconstruction. Test dependency/stdlib symbol leak, conflicting prefixes, no-prototype/variadic, external-inline-only, anonymous params, tag identity, narrow integer, Boolean, float and target mismatches, stale overrides. Demonstrate automatic clamp declarations plus authored low/high guard; coordinate actual inline and rich examples with W07–W09.

**Done:** one header change regenerates exact data, real LLSynth accepts generated output, identical results across checkout locations, no invented signature or safe flag; unsupported outputs explicit; no second manual declaration inventory.

### W04 — Standard FFI inventory, manifest ownership and regeneration CI

**Owns:** `on1x/stdffi/README.md`, `stdffi/mulib/` generated files and `index.json`, inventory adapter and coverage/freshness CI; no direct edits to global `MANIFEST.yaml` without coordinator. **Depends on:** W03 generation API, W02 provider, W05 compiler format.

Move from `stdlib/ffi` to `stdffi/mulib`; relocate semantic overrides with profile owners. Enumerate actual registered public headers and special recipes through the real manifest. Account for every entry, including header-only/data-only/CDD/multi-header/ambiguous modules. Use bounded parallel generation with stable ordering, explicit failures, dependency digests, staging/atomic index, stale deletion detection and no timestamps. Partition compiler packages instead of raising 64-file/256-export limits blindly; cooperate with W05 indexed registry. Ensure fresh check does not mutate files and generated registry is included in installation. No hand editing generated files or duplicating profile semantics. Publish counts: total registered, attempted, projected, extracted functions, representable, callable unsafe/safe (from actual admission), unsupported and reasons by target. Counts must reconcile; separate module from symbol counts.

**Done:** current manifest fully accounted for, real output regeneration (at least clamp and qualified inline profile), check mode reproducible, unsupported modules represented accurately, all paths canonical and old directory no longer active. Full inventory regeneration is required for final completion; tool limits/unsupported headers may remain explicitly non-callable.

### W05 — FFIHandler DSL, genuine LLSynth, typed descriptor compiler

**Owns:** `FFIHandler.llsynth`, `tools/ffic.py`, semantic table compiler (split file as needed), FFIHandler CMake, `INTERFACE.md`, `include/on1x/ffi.hpp`/transitional `ffi_scalar.hpp`, immutable registry in `ffi.cpp`. **Depends on:** schema agreement with W01/W03/W06–W09.

Implement one grammar for generated declarations, discovery/unsupported entries and authored semantic deltas (or restricted override start form with the same grammar). Use genuine LLSynth output; no fixture-parser fallback. Preserve seed syntax where practical and reject unsupported versions/operations. Emit immutable typed descriptors and exact type/nominal/foreign identity, bounded marshalling plan, source/target provenance. Validate duplicates, symbol/provider references, operation types, budgets, initialized slots and target identity. Split generation per module or deterministic shard and index lookups; only callable profiles contribute executable descriptors. Update CMake and packaging to `stdffi/mulib`, deterministic dependency changes incl removed files; no runtime LLSynth. If prior bootstrap/no-generator configurations require support, use verified genuine snapshots or explicitly report compatibility blocker, not a fake parser. W05 is sole shared schema/CMake writer.

**Done:** real parser, real generated files and validated descriptor compiler pass; grammar unsupported/invalid, forged safe metadata, schema limits, duplicate/collision and reproducibility tests; stable typed API frozen before G2; no parallel scalar engine at final gate.

### W06 — Generic loader, canonical HIR, verification and MEMGraph

**Owns:** `src/loader.cpp`, `src/infer.cpp`, relevant parser/AST/clone/wire fields, `src/sema.cpp`, canonical verifier files, `hir/FFIHandler/verify.cpp`, foreign portions of `hir/MEMGraph`, `hir/lower_mir.cpp` ingress, transitional `include/on1x/kernel.hpp` (W12 deletes after handoff). **Depends on:** W05 descriptor API; W07–W09 request primitives.

Resolve compiler-owned descriptor identity by exact module/member, not prefix/hardcoded function names. Elaborate typed foreign calls and sealed nominal resources, preserve alias/shadowing and lexical unsafe/direct-call restrictions. Revalidate at all ingress including HIR/PPX/wire/clone, enforce ownership/loan/effect/initialization checks even with optional MEMGraph off. Remove foreign admission rules duplicated in MEMGraph only after equivalent FFIHandler checks; retain canonical memory semantics. Prevent optimizer from upgrading trust or duplicating/moving unknown effects. Reject unsupported foreign MIR/VM/Archon explicitly. Avoid per-profile branches; extend generic primitives for W08/W09. Coordinate C11 ingress with W07.

**Done:** new supported generated declaration reaches HIR without changing compiler source; existing clamp/numerical/IPC semantics pass; forged HIR, wrong types/arity/nominal identity, unsafe context, alias collision, ownership/loans, malformed output/status and stale IDs reject; disabled optional optimizer parity.

### W07 — Generic C11 lowering and scalar/static-inline vertical slices

**Owns:** `hir/FFIHandler/lower.cpp` and emitter headers, `src/codegen.cpp`, scalar/inline overrides in `stdffi/overrides/mulib`, corresponding fixtures. **Depends on:** W05 schema, W06 canonical checks, W03 generation, W10 native planning.

Implement generic typed adapter operations, exact C signature assertions, header inclusion and inline forwarding. Evaluate arguments left-to-right exactly once; generate precondition traps at the same points as existing calls. Migrate all three clamp functions with real header-derived declarations and override low/high guard; preserve saturating arithmetic and exit/trap semantics. Add a second real profile, `mulib_media_timebase_gcd` (`uint64_t(uint64_t,uint64_t)` static inline), after U64 support; if full header projection is blocked, report exact blocker and qualify another real supported inline kernel without editing headers to hide failures. Implement generic typed conversion, fieldwise record, slice/out/status and runtime bridge operations requested by W08/W09; these workers supply data and tests, not separate codegen. Keep exact native prototype separate from surface signature; demonstrate mismatched primitive even if same width rejects. Preserve source locations, cleanup order, and safe/unsafe call context. Coordinate wrapper and link-only symbol generation with W10.

**Done:** real Cephyr projection -> `.ffi` -> real LLSynth -> On1x source -> native executable for clamp and one supported static-inline case; host GCC/Clang strict C11 oracle, edge/guard/ABI negatives, reviewed clamp parity with explicit authority; no hardcoded migrated symbol dispatch. A new supported function is added without changing compiler C++.

### W08 — Numerical FFI: records, enums, slices, outputs and status

**Owns:** authored `stdffi/overrides/mulib/{histprice,priceopt}.ffi`, numerical examples/oracles and profile tests. **Depends on:** W01–W07 schema and generic adapters; W10 trust/native plan.

Extract real signatures via Cephyr; audit existing numerical adapters, manuals, review subjects, enum domains and error behavior. Preserve nominal result identities, field ordering, bounded immutable slice extents and validated data/offset/length, float checks and native types. Map status to result sums; only read initialized output on the correct branch. Request generic primitives from W05–W07 instead of writing domain switches. Preserve `libm`, shared implementation closure and multi-kernel review subject set. Test mismatched lengths, invalid enum, narrowing, floating edge cases, trap/evaluation order, output poisoning/failure, independent C oracle and real On1x execution. No unsupported direct aggregate ABI; use fieldwise adapters and C compiler layout tests.

**Done:** both existing numerical profiles and combined history/pricing workflow retain results, errors and independent review gating; generated signatures replace duplicate manual definitions; same generic lowering used; host qualified, Cephyr explicit unsupported cases documented.

### W09 — Resources, ownership and framed IPC/callbacks

**Owns:** resource/IPC semantic override files, existing framed IPC runtime bridge and resource-specific fixtures. **Depends on:** W05–W07 generic schema, canonical ownership and emitter; W10 native trust.

Use actual existing IPC bridge declarations (including private runtime companion if not a canonical MuLIB header). Do not present synthetic IPC bridge as a public header-generated kernel; represent it as a bounded explicit companion recipe linked to real canonical modules. Migrate existing one-shot and reusable framed transport, sealed resource IDs, exactly-once cleanup, exclusive call-scoped session loans, same-owner-thread callback registration/revocation, status results and early exits. Preserve no pointer escape, no foreign-thread callback and scheduler restrictions. Keep runtime SDK ABI unless explicitly approved. Audit JSON/Vec/Parsec/Mune and other prior sidecars, preserve missing obligations in same FFI DSL or explicit unsupported discovery records. Add one further bounded resource example only if fully supported and verified. No inferred ownership from pointer qualifiers or naming; no generalized async/callback support by fiat. Request canonical and emitter operations from W06/W07.

**Done:** old IPC regression parity and ASan/UBSan applicable cases, cleanup once, wrong owner/loan/thread/revoke/escape/use-after-move/failure negatives, generic resource semantics without per-domain HIR/codegen paths. Unsupported pointer APIs remain non-callable and are counted honestly.

### W10 — Native planner, ABI, unified trust, CLI and project integration

**Owns:** `tools/on1x-build.py`, `tools/on1x-native-build.py`, existing Cephyr kernel helper, reusable native planner portions of `on1x-exotract.py`, new common FFI trust implementation, project driver/CLI, `stdlib/on1xfile` wiring, install/runtime artifact policy. **Depends on:** W02 provider and W05 descriptor identity; coordinates W04 manifest adapter and W07–W09 profiles.

Implement one descriptor-driven native plan with actual canonical manifest closure, real implementation switches/companions/external libraries, static and DSO partitions, header-only placement, exact compiler/target options, target-specific ABI assertions and independent probes. Use explicit host/Cephyr provider; no fallback. Reuse previous review verifier semantics in a small normalized FFI trust policy; exact subject binds assumptions, headers, target, flags, compiler, descriptors, adapters and artifacts. Preserve current independent operator-selected authority and negative tests, exact DSO identity and load policy. No generated file authorizes itself. Reuse existing project lock/frozen/offline/sandbox behavior; keep old import syntax/CLI aliases only as compatibility frontends, not a parallel pipeline. Stage and install `stdffi` and required native inputs; actual compile only selected imports. Provide `--inspect-ffi` or equivalent report and migration CLI mappings (proposed names, align with live driver). Native planner remains usable without Exotract/ExoML extraction after migration. Do not delete old entry points before W12 gate.

**Done:** previous static/dynamic/reviewed/unsafe native workflows pass; clamp host-host, Cephyr adapter/host implementation and independently embedded Cephyr adapter/kernel tested where supported; actual wrong prototype, stale target, wrong key, tampered artifact and unsupported provider reject. Combined plans dedup switches and maintain correct native partitions. Project/installation tests pass; no dependency on legacy declaration extraction in new path.

### W11 — Independent qualification, negative controls and regression gates

**Owns:** independent `hir/FFIHandler/tests/integration/` or repository-standard harness, baseline and final evidence, qualification matrix; not production implementation. **Depends on:** all workers for runnable products, begins at G0.

Capture old tests before migration. Run real upstream Cephyr interface tests, actual MuLIB header projections, real LLSynth generation and actual On1x compile/run across relevant host and Cephyr configurations. Add synthetic declaration edge tests only as supplements, never substitutes for real headers. Verify all gates in §7–8, including malformed wire, stale inputs, no-origin export selection, target-specific type identity, negative compilation/link cases, wrong scope and trust forgery. Use independent oracles and mixed compiler boundaries, exact outputs/traps, sanitizers, actual DSO artifacts and installed tool. Reproduce known negative control on baseline or deliberately faulty fixture; record why it must fail. Verify no test filters silently match zero tests; count all attempts. Test both old and new path before deletion and rerun after deletion. Distinguish projection coverage from executable target qualification.

**Done:** evidence for every claimed capability, clean-build reproduction, exact command logs, pass/fail/skip totals, explicit blocked cases, signed-off gate decisions. Do not approve retirement solely from worker reports.

### W12 — Legacy removal, dependency audit and user documentation

**Owns:** retirement manifest, obsolete On1x-only contract/KernelBind tools, sidecars and docs after owner handoff; final public FFI docs/examples and migration mapping. **Depends on:** W03–W10 parity and W11 gate approval; inventory starts immediately.

Inventory every old tool/API/CMake option, hardcoded symbol path, contract catalogue, review fixture, documentation and non-On1x consumer. Identify where each real capability moves. Delete old implementation only after replacement evidence, not by changing names. Remove active `on1x/stdlib/ffi/mulib` registry, `contracts-exoml/` and `contracts-exotract/` On1x-only duplicate catalogues, old `on1x-bind`/`on1x-contracts.py` paths, obsolete extraction reports, dedicated KernelBind dispatch and stale build switches when no verified behavior depends on them. Factor useful native planner or trust verifier instead of deleting substantive checks. Preserve historical records as history, migrate all useful tests, keep shared LibStone and non-On1x tools untouched. Do not remove ExoML/Exotract from LibStone itself. Preserve necessary explicit compatibility alias (one implementation). Update build/install/docs and run grep/dependency audit for residual active paths. Report exact blockers if a legacy bounded path remains.

**Done:** clean configure/build/install with new architecture and retired dependencies absent, complete old-to-new command/file map, no parallel active binding/admission mechanism for migrated profiles, W11 reruns equivalent regression gates. No false completion if a qualified old path cannot yet be removed.

## 7. Dependency-aware integration gates

| Gate | Integrate | Required evidence before advancement |
|---|---|---|
| G0 — baseline | Coordinator + W11 | Exact pins, clean/dirty inventory, baseline tests, existing v0 reconciled, selected Cephyr feature verified or explicit blocker |
| G1 — projection and schema | W01/W02/W03/W05 | Real Cephyr semantic output, strict reader/target checks, deterministic format, exact native vs surface model, override contract frozen, real LLSynth parses initial generated clamp |
| G2 — scalar vertical slice | W04/W06/W07/W10 | Real clamp extraction and generated `stdffi/mulib/clamp.ffi`, guard parity, HIR/unsafe/review checks, native host reference, qualified real static-inline case, no per-symbol scalar dispatch |
| G3 — inventory and rich profiles | W04/W08/W09 plus W05–W07 | Full inventory attempted and reconciled, stable per-module generation, numerical/IPC parity, ownership/status/error negatives, explicit unsupported cases |
| G4 — native policy and project | W10 + W11 | Static/DSO, explicit Cephyr/host, independent ABI probes, unified trust, project and installed compiler, no normal ExoML/Exotract extraction dependency |
| G5 — removal | W12 + W11 | Old-to-new dependency audit, gated deletions, clean build and equivalent tests after removal, historical records not counted as active implementation |
| G6 — release | Coordinator + 12 handoffs | Full suites, deterministic freshness, clean install, exact scope/provenance, residual blockers, final migration status |

W01 and W02 work concurrently on existing producer/consumer. W03 and W05 freeze shared representation/grammar together; W04 starts manifest and stale-file inventory before G1. W06/W07 design against schema early, then integrate; W08/W09 author overrides and tests concurrently after signature projection. W10 starts native/trust audit at G0 and wires staged profiles; W11 continuously verifies; W12 prepares dependency/removal map from day one. Do not serialize the entire migration merely because some integrations have dependencies. Do not merge partially updated schemas or shared generated registries from independent workers; coordinator regenerates at integration gates and records exact revisions.

## 8. Mandatory qualification matrix

Use actual CMake/CTest options from checked-out source. Example On1x baseline/build command (adjust paths and optional feature flags to real source):

```sh
cmake -S on1x -B build/ffi-real \
  -DON1X_LIBSTONE_ROOT=/abs/path/to/selected/libstone \
  -DON1X_REQUIRE_LIBSTONE=ON -DBUILD_TESTING=ON
cmake --build build/ffi-real --parallel 4
ctest --test-dir build/ffi-real -N
ctest --test-dir build/ffi-real -R '^on1x\.ffihandler\.scalar$' --output-on-failure
ctest --test-dir build/ffi-real --output-on-failure
# From the selected LibStone checkout, after building actual Cephyr:
python3 cornerstone/Cephyr/tests/test_c11_interface.py /abs/path/to/cephyr
```

Discover actual Cephyr configuration, test names and new FFI tests. A zero-test filter is failure to qualify, not success. Run separate host GCC and Clang builds and appropriate real Cephyr builds. Preserve existing On1xMETA/MEMGraph generation and optional-off checks. Record fallback/bootstrap compatibility (genuine generator only); installed application must not need generation tools. For ASan/UBSan, record flags, target and runtime scope. For cross-target claims distinguish projection, compile/link, and actual target or emulator execution.

| Area | Required positive and negative evidence |
|---|---|
| Upstream interface | Actual upstream fixture for three built-in targets; direct C, preprocessed and C11Syntax input parity; wrong stage/version/target/declaration constraints reject; declaration-valid/body-invalid case remains non-callable |
| Reader/schema | Quoted vs atoms, JSON escapes, integer bounds, unknown values, tag ID 0, recursive pointers and distinct same-name tags; malformed/truncated/deep/oversized, duplicate clauses, dangling refs, impossible layouts and trailing forms reject |
| Ownership of exports | Real headers and includes; no libc/dependency symbol leakage, ambiguous origin fails closed, canonical alias collision, external inline-only vs static inline, no fake macro/object exports |
| Type mapping | Native rank/spelling/size/sign/alignment, char signedness by target, Bool, floats/long double, adjusted array/function params, `f()` vs `f(void)`, enum/layout/unknown and bit-field handling; same-width incompatible primitive rejects |
| Generation | Real header -> projection -> actual `.ffi`; override guard retained, stale override rejects; per-module/full freshness, transitive include and macro drift, deterministic independent checkout locations, failure preserves prior good outputs; no manual generated edits |
| LLSynth | Real parser and descriptor compiler; grammar/version/duplicate/unsupported inputs, budgets, removed source dependency invalidation and package sharding; no test-parser fallback |
| HIR/semantics | Correct nominal/type identities, direct alias and lexical unsafe, forged HIR/PPX/safe metadata/stale ID reject; once-only LTR evaluation, traps, optimizer and MEMGraph-off behavior |
| Scalars | Actual clamp saturating edge oracle and reversed interval trap; qualified unsigned static inline oracle; executable generated On1x code, not only C wrappers |
| Numerical | Actual Histprice/PriceOpt combined workflow, enum/record order, slice lengths/offsets, output initialization and error branch, `libm` plan, independent C oracle |
| Resources | Existing IPC one-shot/session tests, exactly-once cleanup on error/early exit, stale loans/wrong owner/thread/escape/double drop and callback revoke negatives; applicable ASan/UBSan |
| ABI/native | Actual C headers and exact `_Generic` prototypes/layout checks; independent host and mixed Cephyr caller/callee probes; wrong primitive/symbol/layout/target reject; native linked artifact verified |
| Trust | Existing reviewed static/DSO calls with explicit test authority; wrong/stale key, signature, assumptions, headers, flags, compiler/target, artifact and linkage all reject; never self-authorized |
| Planner | One implementation TU per partition, dedup switches, native companions, actual external libs and static-inline forwarding; no silent provider fallback, no partial publication |
| Backend | Direct C11 reference; MIR/VM/Archon reject unimplemented foreign semantics; no hidden fallback |
| Project/install | Actual On1xfile build, lock drift, offline/frozen/sandbox, staged inputs, installed compiler and generated stdffi packaging; no generator runtime dependency |
| Retirement | Fresh build/install after gated deletions; every previously qualified behavior tested, old APIs mapped, no active duplicate extraction/contract path, non-On1x consumers preserved |

Minimum end-to-end release demonstrators:

1. `include/mulib-clamp.h` -> real Cephyr `--emit-interface=sexp` -> auto-generated clamp interface + small authored precondition -> real LLSynth -> HIR -> actual unsafe and separately authorized reviewed On1x call -> existing clamp oracle. Test both static and previously supported dynamic modes.
2. Real static-inline header -> generated U64 (or another truly supported scalar) descriptor -> actual local/extern forwarding adapter -> On1x executable and independent oracle. No invented exported symbol or implementation switch.
3. Real numerical kernel(s) -> generated declarations + bounded overrides -> generic slice/record/status marshalling -> existing reviewed combined workflow.
4. Existing framed IPC -> generic sealed resources and callback lifecycle -> old positive/negative tests with sanitizer where applicable.
5. Add one supported newly declared function/profile through header/manifest and optional semantic override changes plus tests, regenerate `.ffi`, build and execute without editing compiler C++ or hand-authoring extracted signatures. Use a real existing header or controlled fixture; do not modify public APIs solely to create a demo.

## 9. Retirement boundary and non-goals

Final architecture contains Cephyr as C declaration projection provider, LLSynth/FFIHandler as DSL/descriptor compiler, canonical On1x HIR verifier and MEMGraph for memory semantics, one native planner and explicit trust policy. Generated standard descriptions live only under `on1x/stdffi/mulib`. No competing On1x CDecl extraction, contract catalogue or KernelBind engine. Remove normal libclang dependency attributable solely to FFI extraction after parity, not unrelated compiler capabilities. ExoML/Exotract remain independent LibStone technologies. Keep existing native ABI probes, canonical ownership, necessary runtime bridges and independent review verification, not the surrounding duplicate machinery.

Not required: general safe C FFI, arbitrary variadics/pointers/callbacks, inference of ownership/lifetime from declaration syntax, all-target aggregate ABI, new MIR foreign operations, native async, cross-language LTO, arbitrary dynamic loading or complete network admission. Every registered MuLIB module must be accounted for in generation; not every function must become callable. Explicit unsupported is an acceptable inventory result; unsupported previously qualified behavior is a migration blocker unless an authorized breaking change is recorded. No broad trust expansion just because generation succeeded.

## 10. Final delivery and definition of done

Coordinator delivers real integrated commits, generated standard library, runnable tools and tests, and `on1x/hir/FFIHandler/MIGRATION_STATUS.md` with:

1. Exact MuLIB/LibStone revisions, selected pin vs upstream feature revision, clean/dirty state, tool binaries/versions/digests, build config and actual worker/merge order.
2. Manifest inventory coverage and generated file count; modules/functions discovered, attempted, projected, representable, callable-unsafe and callable-safe, by target and provider; all failures/skips and unsupported reasons reconciled. No claims based on historical header totals.
3. Actual generated `.ffi` paths, provenance/index and override policy, deterministic `--check` result, incremental/transitive-drift tests and reproduction commands.
4. Actual tests run with logs and exact counts, independent ABI probes and native artifact identities, sanitizer scope, verified vs projection-only targets, real LLSynth evidence, all baseline/new failures and skips.
5. Old-to-new files/commands/options map, deletion list, intentional compatibility aliases and remaining active legacy blockers; clean install with expected dependencies.
6. Existing callable profile parity and explicit trust policy, with no production approval claimed. Demonstrate descriptor-only extension through real Cephyr -> generated `.ffi` -> real LLSynth -> actual On1x execution.
7. Each remaining capability scoped as VERIFIED (evidence-backed tested claim), EXECUTABLE (actual limited implementation), PARTIAL (specified gaps), or ABSENT (not implemented). Never use those labels as safety certificates. Release recommendation: complete only if all gates pass, otherwise partial and exact corrective work.

**Definition of success:** a maintainer changes a MuLIB header, runs the generator using Cephyr's real semantic projection, obtains deterministic `on1x/stdffi/mulib/*.ffi`, and the supported interface becomes usable via the generic On1x FFIHandler without compiler source changes. Nontrivial foreign semantics are small explicit overrides; unrepresentable declarations are visible but uncallable; safe calls remain independently authorized. The old On1x KernelBind/contract architecture actually disappears after parity, not merely gets renamed.
