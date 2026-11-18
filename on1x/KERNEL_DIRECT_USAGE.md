# KERNEL_DIRECT_USAGE.md — Direct MuLIB Kernel Usage in On1x

> **Implementation mandate for an autonomous coding agent.**  This document specifies
> a native On1x language extension for importing canonical MuLIB C11 header-only
> kernels through LibStone **Exotract / ExoLib** library profiles and **ExoML**
> semantic contracts. Implement the described features in working, tested code;
> do not substitute a planning document, mocked adapters, automatically trusted
> extern declarations, or renamed C shims for a functioning implementation.
>
> **Suggested location:** `mulib/on1x/KERNEL_DIRECT_USAGE.md`.
>
> **Primary repository:** `https://github.com/Chubek/mulib` (`on1x/`, `include/`,
> `MANIFEST.yaml`, `library/`).
>
> **Contract/toolchain repository:** `https://github.com/Chubek/libstone`
> (`moonstone/exotract/`, `moonstone/exoml/`, `moonstone/chisel/`).
>
> **Proving application:** `https://github.com/Chubek/shylock`.
>
> **Status:** Design and acceptance specification, **not** an assertion that
> `import kernel`, general safe foreign-resource imports, or the illustrated
> high-level wrappers exist in the current compiler.
>
> **Verified implementation:** [KernelBind discovery](docs/kernel-bind.md)
> parses native import requests, resolves canonical profiles, and extracts
> source-pinned CDecl v2 and applicable ExoML sidecars. A separate opt-in
> C11 profile executes `unsafe import kernel mulib.clamp` for the two
> saturating `i64` operations. Safe and resource-bearing imports remain
> unadmitted. PrettyPrint is covered
> by `make -f prettyprint.mk test-kernel-discovery`; its
> [stdlib migration](stdlib/prettyprint/README.md#direct-kernel-import-status)
> is blocked on callable lowering for the renderer's record borrow, enum,
> null failure, and owned string result, as well as safe admission.

**Implementation update — Packages A/B, discovery only.** The current compiler
parses distinct contextual kernel requests and reports them with `--kernel-imports`.
`on1x-build.py --inspect-kernels` explicitly uses the bundled ExoLib recipe and real
Exotract/ExoML CDecl v2 for six canonical kernels; the existing JSON sidecar
remains unreviewed and the other profiles have no semantic sidecar.
Both safe and unsafe kernel imports reject compilation, including through an
ordinary dependency. No fake or raw binding is synthesized. See
[the implemented profile](docs/kernel-bind.md) and [measured evidence](VERIFICATION.md).
The Package C scalar runnable milestone and §9.5 Shylock gate are unmet.

## 0. Objective and non-negotiable invariants

Implement a *compiler-native* facility, **KernelBind**, through which an On1x
program can select, type-check, call, and link C11 header-only MuLIB kernels
without application-authored `extern fn` declarations, C pointer plumbing,
`MULIB_IMPL_*` switches, or Shylock-specific native bridges.

The canonical programmer-facing syntax is:

```on1x
import kernel mulib.statsprice as Stats;
import kernel mulib.histprice as History;
import kernel mulib.priceopt as Pricing;
import kernel mulib.money as Money;
```

Once a function has been admitted into an On1x-safe interface, it is called
with ordinary On1x expressions; **the exact high-level function names are
compiler-generated, versioned API surfaces** documented by the binding output.
For example (illustrative final API, not currently executable):

```on1x
let result = History.holt(samples, 0.4, 0.2, 0.9, 3)?;
let decision = Pricing.optimize(demand_model, constraints)?;
```

### Rules

1. **ExoML is the declaration and semantic-contract authority.** Use the real
   compiler-derived ExoML CDecl v2 graph and ExoML v2 semantic contracts. Do
   not invent a separate C parser, C ABI schema, or parallel ownership ontology.
2. **ExoLib describes the library**, not the application. Use Exotract's existing
   `exotract.exolib.new(...)` mechanism for canonical header, implementation
   switches, include paths, dependencies, compiler profile, sources and metadata.
3. **Exotract discovers and builds** the actual MuLIB kernel implementation
   closure and import metadata. It does not assert safety by discovery alone.
4. **On1x admits and lowers** checked foreign signatures and their resource,
   lifetime, effect, error and capability obligations. An Exotract manifest by
   itself does **not** authorize safe calls.
5. **Safe by default, fail closed.** No guessed type layouts, unchecked casts,
   fabricated `safe_export`, silently discarded obligations, unchecked callbacks,
   or implicit fallbacks to raw `extern fn`.
6. **Exactly one MuLIB implementation translation unit** per link product, with
   the transitive `MULIB_IMPL_*` definitions selected according to the real
   manifest/ExoLib recipes. Companion files remain separate when required.
7. **Application sources stay On1x.** Generated and audited C11 binding adapters,
   the generated MuLIB implementation TU, and ordinary runtime/kernel C are
   permissible; no handwritten Shylock-only native adapter should be necessary.
8. **Preserve established On1x syntax and behavior**—ordinary `import`, modules,
   nominal ADTs, `Result` and `?`, function-scoped Go-style `defer`, PPX hygiene,
   and existing foreign escape hatches. This is additive, not a language rewrite.
9. **Never claim implementation from a header's mere existence.** Every supported
   foreign shape requires compilation, link and execution evidence against the
   exact public C declaration and actual MuLIB function.
10. **Be honest about trust.** Existing shipped ExoML sidecars in MuLIB include
    annotated/unreviewed contracts. A hash, AI-written annotation, a passing
    probe, or a user-written `reviewed` string is not an independent semantic
    safety proof. The safe-import gate must be auditable and refuse unadmitted
    profiles.

## 1. Reinspect the live implementation before coding

Check the checked-out revisions and update the implementation log; do not
assume the following observations still hold after repository changes.

- `on1x/grammar/On1x.g` and `On1x.asdl`: currently implement the scanner/token
  arena; they are **not** the complete source parser or checked language AST.
- `on1x/src/parser.cpp`, `on1x/include/on1x/ast.hpp`: the actual source parser
  and checked AST representation; `ast.hpp` currently has an `Import` structure.
- `on1x/src/{lexer.cpp,lexer_fallback.cpp,loader.cpp,infer.cpp,sema.cpp,
  safety.cpp,codegen.cpp,cli.cpp}` and `on1x/src/backend/{lower.cpp,
  emit_c.cpp,mir.cpp,passes.cpp}`: inspect the available pass boundaries and
  supported HIR/MIR profiles before modifying them.
- `on1x/tools/{on1x-build.py,on1x-exotract.py,on1x-contracts.py}` and
  `on1x/contracts-{exotract,exoml}/`: preexisting extraction/build/admission
  workflows; reuse them instead of starting a duplicate generator.
- `on1x/stdlib/mulib/{Native.on1x,README.md,housekeeping.py,catalog.json}`:
  selected-kernel metadata and build planning are not general safe ABI bindings.
- `on1x/stdlib/_bridge/`: existing trusted raw scalar C adapters; do not
  mistake them for independently reviewed ExoML safe exports.
- `on1x/docs/{contracts.md,ffi-strings.md,shylock-readiness.md,
  memory-safety.md,concurrency.md,nominal-adts.md,modules.md}` and
  `on1x/COMPLETION.md`: existing capabilities and explicit blockers.
- LibStone `moonstone/exotract/{DESIGN.md,README.md,binders/on1x.lua,
  lua/exotract/README.md}`: existing Lua ExoLib and On1x metadata binder.
- LibStone `moonstone/exoml/{include/exoml/{CDecl.hpp,Contract.hpp},
  contracts/schema.exocontract.json,manual/semantic-contracts.md}`:
  canonical source/semantic models.
- MuLIB's actual `MANIFEST.yaml` and the public `include/mulib-*.h`
  prototypes for every kernel selected in a test.

Baseline expected from the previously inspected checkout: Exotract's On1x
binder emits `moonstone.exotract.on1x/2` **typed metadata**, with
`unsafe_by_default:true`, **not executable On1x declarations**. On1x's safe
import admission is narrow (scalar and separately reviewed immutable-string
profiles), not general slices, records, handles, out-params or callbacks.
The On1x Shylock readiness document explicitly marks general executable and
safe-FFI requirements incomplete. Verify any newer work instead of relying on
this snapshot.

Record git SHAs, compiler/target ABI, relevant test results, and any preexisting
failures before starting. Do not patch the canonical kernels just to make
invalid bindings appear correct.

## 2. Language surface: intrinsic kernel imports

### 2.1 Required grammar (v1)

```ebnf
kernel_import
  ::= "import" "kernel" kernel_path ["as" identifier] ";"
   |  "unsafe" "import" "kernel" kernel_path ["as" identifier] ";"

kernel_path
  ::= identifier { "." identifier }

unsafe_expression
  ::= "unsafe" block
```

**Examples:**

```on1x
import kernel mulib.histprice as History;
import kernel mulib.json as JSON;
unsafe import kernel mulib.microjs as MicroJS;

fn main() {
    // An admitted high-level function can be called normally.
    // A raw foreign operation requires an explicit unsafe context.
    unsafe {
        // MicroJS.<raw_export>(...);
    }
}
```

`kernel` and `unsafe` should be contextual rather than globally reserved where
that preserves existing identifiers and parser compatibility. Existing scanner
rules already handle the identifiers and punctuation needed; update both
Aurocks-generated and fallback scanner paths only if the chosen grammar
actually requires it. Retain source offsets, module-local feature activation,
macro hygiene and diagnostics. Do not parse these imports through a PPX.

### 2.2 Name resolution

- `mulib.histprice` is a stable logical module identity, **not** a filesystem
  path or a request to perform arbitrary network fetching.
- Resolve against explicit trusted kernel roots, ExoLib profiles, and MuLIB's
  canonical `MANIFEST.yaml`/selected module registry using existing On1x
  search-path rules. An unknown or ambiguous module fails.
- Bind `History` to a compiler-generated *module namespace* with typed
  functions, types, constants and checked enum cases. Never make these into
  unqualified global C symbols.
- Preserve the C symbol name (`mulib_histprice_holt`) in generated metadata.
  A friendly alias such as `History.holt` is allowed only via a deterministic
  documented naming policy: strip the verified module prefix if unique;
  otherwise keep the full name or require an explicit alias. Reject collisions.
- Imports are compile-time static links, consistent with existing On1x modules;
  **do not** implement runtime dynamic loading as a side effect of this syntax.
- `import kernel` must not silently import only the subset a partially verified
  contract happens to support. Define whether a whole module is admitted or
  explicit selected exports are admitted, and issue a diagnostic for requested
  unsupported functions. For v1, an import can expose only admitted members,
  with introspection of rejected members; using one must fail with the reason.
- A future additive extension may support `from kernel mulib.histprice import
  holt as forecast;`; do not make this a prerequisite for the v1 vertical slice.

### 2.3 Unsafe semantics

`unsafe import kernel ...` exposes *ABI-verified raw operations* that lack full
semantic admission, not arbitrary dynamically typed C invocations. Calls must
occur in explicit `unsafe { ... }` context, carry an `unsafe_foreign` effect,
remain visible to reviewers and instrumentation, and retain all checks the
compiler can establish (symbol identity, calling convention, argument count,
layouts, source/target pinning). If a C shape cannot be lowered exactly, even
unsafe calls are rejected. `unsafe` never fabricates a safe destructor, thread
policy, pointer lifetime or callback trampoline.

Retain compatibility with ordinary existing `extern fn` programs, but do not
use manually written `extern fn` declarations to implement the v1 success path.

## 3. The KernelBind compiler subsystem

Implement a distinct compiler pipeline, not a source-level macro that writes
unchecked native declarations:

```text
On1x source: import kernel mulib.histprice as History
             |
       module/name resolution
             |
    trusted ExoLib profile / MuLIB MANIFEST closure
             |
    Exotract C11 discovery (ExoML CDecl v2)
             + ExoML v2 semantic sidecar
             |
       provenance/schema/ABI validation
             |
   contract and import-admission classification
             |
    generated typed On1x module interface
             |
    On1x type/effect/ownership/borrow checking
             |
    typed foreign-call HIR / verified lowering
             |
   generated C11 adapters + one MuLIB implementation TU
             |
     C11 + On1x runtime + canonical MuLIB archive
```

### 3.1 Suggested compiler representation

Extend the checked AST with a dedicated import variant or import kind, e.g.
`KernelImport{where,module,alias,is_unsafe,profile_identity}`; do not overload a
relative source-file import and infer kernel status from its spelling. Source
nodes must not contain forged admission certificates. Introduce a separate
compiler-generated `ForeignModuleInterface` with:

- canonical module identity, C symbol identity and stable generated names;
- target-triple/C compiler flags/preprocessor/ABI fingerprints;
- source/header/manual/manifest and ExoLib/ExoML fingerprints;
- complete CDecl type IDs and canonical function signatures;
- exact imported On1x types and rejected type-shape diagnostics;
- resource owner/borrow/transfer/alias/retention/lifetime relationships;
- known/unknown effects and thread/callback/suspension constraints;
- exact error-convention mapping and output-initialization rules;
- adapter/link artifact identities and admission evidence schema/version;
- trust classification: discovered, annotated, reviewed, admitted, unsafe.

Keep the canonical ExoML graph available as an authoritative input. The
compiler-facing interface can be a versioned *projection* with references to
canonical CDecl type IDs and semantic-contract IDs, not a competing contract
language or an editable pseudo-proof.

### 3.2 Trust and admission gates

Required for a safe import, in order:

1. **Discovery:** Re-run or verify Exotract's complete target-specific CDecl
   extraction. Header-only implementation macros must be *disabled* in this
   declaration pass. Unknown/incomplete types, macros, symbols or layouts fail.
2. **Identity:** Verify hash and exact identity of kernel revision, canonical
   header(s), preprocessed inputs, ExoLib recipe, `MANIFEST.yaml`, compilation
   flags, target ABI, ExoML sidecar and tool versions. Reject drift.
3. **Contract validation:** Use ExoML v2 canonical validation against the exact
   CDecl source/signature/location; reject contradictions, unknown obligations,
   malformed/duplicate JSON keys and incomplete provenance.
4. **Independent semantic review gate:** Require independently auditable
   evidence and an explicit review policy for any nontrivial claim. Do not grant
   admission because a file declares `reviewed` or `verified`; such fields are
   claims, not their own authorization. Keep unreviewed imports unavailable
   in safe mode.
5. **ABI gate:** Validate calling convention, scalar widths/signs, enums,
   `size_t`, records, alignment, packing, constness, array sizes, callbacks and
   output-parameter layout. Use target-compiled probes, `_Static_assert` and
   `_Generic` where applicable; do not emit unsafe casts as a substitute.
6. **On1x representability gate:** Reject any obligation that cannot yet be
   expressed/enforced by On1x (borrow regions, retained callbacks, owned
   destructors, error mapping, synchronization, C variadics, etc.).
7. **Call-site check:** Run the type/effect/ownership/resource checker on the
   actual post-macro checked program, including generated adapters and the
   real imported interface. Preserve Chisel's independent checks where enabled.
8. **Executable gate:** Compile and exercise the exact adapter against the
   real kernel implementation with representative success/failure and target
   ABI tests. This is necessary engineering evidence, **not** proof of arbitrary
   C implementation behavior.
9. **Publication:** Publish an immutable versioned admission manifest, checked
   generated On1x interface, adapter source/object and build lock only after
   all requisite gates pass. Publication must be atomic and non-overwriting.

A module not satisfying these gates can remain discoverable for diagnostics
or explicitly unsafe usage; it must not become safe by omission of evidence.
Maintain machine-readable `admitted`, `unsafe`, `unsupported`, `missing_review`
and `not_representable` statuses with per-symbol reasons. Do not silently skip
rejected requested calls.

### 3.3 Artifact schemas (new; do not alter ExoML's schema)

Version the *On1x import/admission and link* artifacts separately from canonical
Exotract/ExoML wire formats. For example:

```text
moonstone.exotract.on1x/2      existing metadata input
exoml CDecl v2                 existing canonical declaration authority
exoml semantic contract v2    existing canonical semantic authority
on1x.kernel-interface/v1      proposed typed import projection
on1x.kernel-admission/v1      proposed independently checked evidence
on1x.kernel-build-plan/v1     proposed deterministic link/implementation plan
```

Names are proposed; implement one consistent, explicitly versioned schema
instead of proliferating partially compatible manifests. Schema version drift,
unknown required fields, mismatched revisions and stale fingerprints fail
closed. Never interpret a previous `on1x.admitted-extern/v1`–`v4` certificate as
an automatic authorization for new resource-bearing signatures.

## 4. Foreign type-system additions

KernelBind requires capabilities beyond a new import grammar. Extend On1x's
actual checked type system, value model, resource analysis and C11 emitter.

| Suggested On1x notion | Meaning and required enforcement |
| --- | --- |
| `Owned<T>` | Unique foreign resource with verified constructor and destructor; affine move, exactly-once release on all supported exits. |
| `Borrow<T>` | Shared immutable borrow tied to a named owner/region; cannot outlive owner or cross incompatible suspension. |
| `BorrowMut<T>` | Exclusive mutable borrow; no conflicting mutable/shared aliases during the borrow. |
| `Span<T>` | Borrowed pointer+length to contiguous elements with validated extent, alignment and lifetime. |
| `Buffer<T>` | Owned bounded contiguous allocation; checked growth/mutation and release. |
| `Opaque<T>` | Nominal foreign handle; user cannot inspect its C layout or do pointer arithmetic. |
| `Result<T,E>` | Typed success/error; map documented C error conventions and valid outputs into this form. |
| `Option<T>` | Nullable/optional result, never confuse null with failure without an explicit contract. |

These are **target APIs/semantic notions**, not claims that current On1x can
parse all of these generic resource types. Use the existing nominal `Result`,
`Option`, records, `f32/f64` and fixed-width numerics where actually supported;
add proper syntax and type checking only for the missing semantics. Avoid
nominal records containing managed/owning fields until aggregate ownership and
lowering are implemented.

### 4.1 Scalar and numeric ABI

- Exact C integer widths and signedness (`int`, fixed-width integers,
  `size_t`, enum representation), binary32/binary64, C `_Bool`, pointer width;
  no assumption that `long`, `size_t` and On1x `i64` are interchangeable.
- Checked `usize`/`isize` and explicit safe conversions; reject negative and
  overflowing sizes before calling C.
- No implicit conversion between minor-unit money (`i64`) and real-number
  forecast inputs (`f64`); preserve currency and scale.
- Preserve IEEE edge cases, including NaN/infinity where APIs permit them.
  Different MuLIB modules can have different parameter domains; use contracts.

### 4.2 Slices and arrays

A C `(const double*, size_t)` parameter pair should produce an On1x
`Span<f64>` or equivalent without exposing raw pointers. The adapter obtains
pointer and length from the live owner in one checked operation, verifies
extent/count/alignment/overflow, borrows for the call's duration, and cannot
retain the pointer unless the contract explicitly allows and the checker models
retention. Mutable output spans require initialized-element bookkeeping and
strong alias constraints. Support binary buffers containing NUL; do not force
all foreign data through NUL-terminated `string`.

### 4.3 Output and C result adaptation

- Pair an out-parameter and status code only after validating the *actual*
  ExoML error/initialization contract.
- Create aligned temporary C storage, initialize it correctly, call C, check
  status, copy/publish only output fields valid for that status, then clean up.
- Map returned `NULL`, integer sentinel, `errno`, boolean status, tagged error,
  or custom classifier according to the *specific function's* contract.
- Refuse output publishing on failure. Never read uninitialized C output.
- Record names and enum domains use the real CDecl type IDs and independent
  layout probes; never cast an On1x record to an unrelated C record.

### 4.4 Foreign lifetimes and cleanup

Integrate ExoML ownership modes (`scalar`, `borrowed_immutable`,
`borrowed_mutable`, `owned_unique`, `shared`, `consumed`, `out`, `inout`,
`retained_callback`, `transferred`, `opaque_unsafe`) and return/destructor
contracts with On1x's existing type/effect and Chisel checking. Enforce moves,
borrows, invalidation after mutation, conditional transfer, destructor family,
thread affinity and double-close rejection. Track generative resource identities
and region relationships in typed HIR, not ad hoc runtime integer handles.

Current `defer` is function-scoped, not a general RAII/`using` primitive.
Implement reliable automatic foreign cleanup and/or a verified explicit release
pattern for every supported normal/error exit. Address terminal traps,
cancellation and suspended calls honestly; reject unsupported unwinding profiles
rather than promising cleanup that cannot happen. `Owned<KVDB>` cannot escape
through untracked aggregate or closure captures.

## 5. High-level wrapper synthesis (contract-driven)

Directly exposing C parameter conventions to On1x is useful for low-level
unsafe consumers, but an idiomatic safe kernel import should synthesize a
high-level interface from contract-approved transformations.

### 5.1 Historical forecasting

The actual public prototype to check is:

```c
int mulib_histprice_holt(const double *prices, size_t n,
                         double alpha, double beta, double phi,
                         size_t horizon, mulib_histprice_forecast_t *out);
```

After exact ABI and semantic admission, a possible On1x interface is:

```text
History.holt(
    prices: Span<f64>,
    alpha: f64,
    beta: f64,
    phi: f64,
    horizon: usize
) -> Result<History.Forecast, History.Error>
```

Final ergonomic API (illustrative):

```on1x
import kernel mulib.histprice as History;
let forecast = History.holt(prices, 0.4, 0.2, 0.9, 3)?;
```

The adapter must check sample length, matching `size_t`, parameter ranges,
allocation/stack bounds and success before reading `out`. Preserve special
semantics: historic EMA and unified price-forecast EMA may differ in whether
alpha zero is permitted; `histprice` may return NaN for unavailable variance.
Do not replace unavailable variance with a fabricated confidence interval.

### 5.2 KVDB owned handle

The real C API includes `mulib_kvdb_open(..., mulib_kvdb_t **database)` and
`mulib_kvdb_close(mulib_kvdb_t *)`; verify its exact return/error semantics.
A completed foreign-resource interface should allow an application pattern
**equivalent to** (future syntax, not currently compiled):

```on1x
import kernel mulib.kvdb as DB;

fn save(path: string, key: Buffer<u8>, value: Buffer<u8>)
    -> Result<bool, DB.Error>
{
    let db = DB.open(path)?;
    defer DB.close(db);
    DB.set(db, key.span(), value.span())?;
    DB.commit(db)?;
    return Ok(true);
}
```

This example is a desired user experience, **not** permission to generate
`Result`/method/`defer` syntax that the current compiler cannot support. The
implementation must document the actual admitted spelling and prove ownership
cleanup. `close` consumes the handle; stale uses fail statically. Preserve KVDB
commit durability/uncertainty semantics rather than returning a generic Boolean.

### 5.3 Callbacks

Do **not** implement callbacks by casting an arbitrary On1x closure into a C
function pointer. Synchronous callbacks require exact C signature trampolines,
userdata lifetime, reentrancy, GC/rooting safety, side-effect policy, and an
explicit call-bound capture region. Retained callbacks additionally require a
registration resource, unregister/destructor semantics, thread-origin and
suspension/teardown restrictions. Start with *callback-free* APIs; leave
retained callbacks unadmitted until fully tested.

For HTML tokenization, provide a generic *pull iterator or bounded event buffer*
in MuLIB if this enables safe On1x use without callback retention. Such a kernel
must be domain-neutral and tested independently; do not hide Shylock-specific
policy inside a new MuLIB header.

## 6. Effects, thread safety, capabilities and concurrency

Preserve ExoML's known effects (`read`, `write`, `allocate`, `free`, `escape`,
`io`, `thread`, `suspend`, `foreign_call`) and unknown-effect state in the
compiler's typed foreign-call nodes. A safe call with unknown or unrepresentable
effects is not admitted. Check blocking and suspension constraints, thread
safety, thread affinity, callback origin and borrowed-resource lifetime.

A contract describing an `io` effect is **not** proof of authorization to use
network, filesystem, credentials or processes. Separate runtime capability
permissions are needed, particularly for Dragnet and Mune extensions. Do not
confuse source-level `unsafe` with an OS sandbox. Preserve existing DNS-pinning,
TLS certificate/hostname validation, redirect/private-address policies and
sandbox isolation when replacing native Shylock transport/VM bridges.

On1x's present cooperative fibers/channels are a bounded single-native-thread
profile, not general typed `async`/`await`, owned resource channels, actors,
native workers or universal nonblocking I/O. Do not claim that direct kernel
imports automatically make blocking foreign calls cooperative. Either provide
a genuinely nonblocking kernel transport with a checked scheduler interface or
reject calls that would block while holding a scheduler-critical resource.

## 7. Kernel build/link integration

Selecting a kernel import should implicitly select its verified canonical
MuLIB implementation closure. Extend existing `on1x-build.py` and Exotract
archive flows, not a second one-TU generator.

1. Resolve logical name to trusted ExoLib/MANIFEST profile (and optional
   `library/` fixture ownership when relevant).
2. Determine transitive header/module dependencies and exact implementation
   switches, target defines, native companion objects and system link libs.
3. Run declaration-only extraction with all `MULIB_IMPL_*` **disabled**.
4. Verify or generate locked CDecl+contract+interface+admission artifacts.
5. Generate exactly one implementation TU with the required switches, plus
   permitted separately compiled companions. Compile to C11 and archive.
6. Compile the application-generated C11, binding adapters and On1x runtime.
7. Link the real archive with required native/system libraries and publish a
   manifest of source/kernel/compiler/target/contract fingerprints.
8. Reject a conflicting implementation switch, duplicate definition,
   unavailable native dependency or incompatible library profile.

Build-time Exotract, Lua, libclang, LibStone C++ and ExoML need not be linked
into the deployed application. Offline compilation should work from locked
local inputs. No compilation step may execute an untrusted fetched ExoLib
script implicitly as a side effect of a source import; contracts and tools are
trusted build inputs, with sandboxing if untrusted configurations are allowed.

Use existing `ON1X_MULIB_PATH`, `--mulib-path`, installed library search roots,
source-pin/archive checks, and `On1xfile` conventions; extend rather than
replace. Library roots must not be able to escape via `..`, symlinks, or
ambiguous shadowed profiles without explicit safe resolution.

## 8. Compiler files to inspect and modify

These are ownership suggestions, not assertions about exact method names in a
particular revision. Minimize changes and preserve existing APIs/tests.

| File/subsystem | Required work |
| --- | --- |
| `on1x/src/parser.cpp` | Parse kernel import/unsafe context, preserve locations and grammar compatibility. |
| `on1x/include/on1x/ast.hpp` | Add distinct kernel-import AST representation. |
| `on1x/src/lexer.cpp`, `lexer_fallback.cpp`, `grammar/On1x.g` | Only if needed for new contextual tokens; retain generated/fallback scanner parity. |
| `on1x/src/loader.cpp`, `discovery.cpp` | Resolve trusted logical names, manifests, imported namespaces and versioned interfaces. |
| `on1x/src/infer.cpp`, `sema.cpp`, `type.cpp` | Foreign nominal types, exact scalar/array/record signatures, errors, unsafe context and effects. |
| `on1x/src/safety.cpp`, Chisel integration | Borrow/move/drop/alias/invalidation/region checking at real callsites. |
| `on1x/src/backend/lower.cpp`, `mir.cpp`, `emit_c.cpp`, `on1x/src/codegen.cpp` | Typed foreign-call nodes, checked C11 adapters, resource cleanup and unsupported-target diagnostics. |
| `on1x/src/cli.cpp` | Import diagnostics, optional binding-plan/admission inspection and offline resolution flags. |
| `on1x/tools/on1x-build.py` | Select, deduplicate and link canonical kernel closure and generated adapters. |
| `on1x/tools/on1x-exotract.py`, `on1x-bind` | Validate metadata and generate executable admitted interfaces, not just JSON inventory. |
| LibStone `moonstone/exotract/binders/on1x.lua` | Where justified, extend *the real* binder to emit additional evidence/interface representation without fabricating safety. |
| `on1x/stdlib/mulib/` | Expose stable admitted kernel namespaces and diagnostics; keep housekeeping metadata separate from callable imports. |

If patching LibStone is necessary, use its actual source checkout and clearly
separate coordinated upstream changes from MuLIB/On1x changes. Do not quietly
fork Exotract, copy private ExoML headers, or invent a duplicate contract
implementation under On1x.

## 9. Tests and acceptance — required, not optional

### 9.1 Parser, loader and module tests

- Valid `import kernel mulib.histprice as History;` and `unsafe import kernel`.
- Unknown library, missing alias/semicolon, identifier collisions, repeated
  imports, mixed ordinary/kernel imports, relative/path traversal rejection.
- Exact diagnostics and locations, bounded token/declaration/manifest budgets.
- Scanner parity: generated Aurocks scanner and fallback scanner produce the
  same results, including malformed inputs.
- No PPX can introduce or forge trusted kernel imports, reviewed contracts,
  type/effect certificates, or already-lowered foreign nodes.

### 9.2 Contract/trust tests

- Complete real ExoML CDecl and ExoLib profile, correct source/target lock.
- Missing/unreviewed/stale contract; wrong C symbol; function arity/layout
  drift; missing input pin; tampered archive; malformed JSON; duplicate keys;
  changed target/triple/flags; unsupported varargs or callback.
- A contract merely marked `reviewed` without independently valid admission
  evidence **must not** become safe.
- Old scalar admission certificates cannot authorize new pointer/resource
  APIs. Explicit unsafe imports retain exact ABI checks and unsafe call sites.
- Atomic, non-overwriting publication and reproducible fingerprints.

### 9.3 ABI, lifetime and numerical tests

- Checked C `size_t`, enum, signed/unsigned and float conversions; 32-/64-bit
  layouts where a suitable toolchain is available; `_Static_assert`/`_Generic`
  probes against the actual headers; strict `-Wall -Wextra -Wpedantic -Werror`.
- Zero length, huge length, invalid alignment, overflows, NaN/Inf, null,
  output-left-uninitialized-on-error, input/output aliasing and mutation.
- Owned handle creation, close, error, double-close, move-after-use, borrow
  escape, concurrent borrow conflict, early `?` return and cancellation cases.
- Differential comparison between direct C and generated On1x calls for
  `mulib_statsprice_estimate`, `mulib_histprice_ema`,
  `mulib_histprice_holt`, `mulib_price_ensemble_blend`,
  `mulib_price_demand_fit`, `mulib_priceopt_optimize` and
  `mulib_priceopt_candidates` **as each becomes admitted**.
- ASan/UBSan and allocation-failure injection; target C11 sanitizer builds.

### 9.4 Link and build tests

- One kernel, multiple kernels, transitive dependencies, implementation
  switch conflict, archive reuse, source drift, symbols missing at link.
- A second compiled copy of the same implementation TU must not silently
  create a valid product; test duplicate-definition rejection.
- Generated app runs with no Exotract/ExoML/Lua/libclang/C++ build-time tools
  at runtime (unless it explicitly uses the MuLIB Mune runtime as a feature).
- Existing ordinary On1x apps, stdlib, PPX, Category, concurrency and
  generated/fallback compiler profiles do not regress.

### 9.5 Shylock proof of usability

Create a *real* multi-module On1x fixture application (initially in
`on1x/examples/` or `on1x/tests/`) that imports selected MuLIB pricing kernels
through the **new syntax** and does all of the following without `shy_*` C
adapters or handwritten `extern fn`:

1. Accept/load a fixture series using an admitted JSON/file API.
2. Convert validated monetary minor units into numerical model inputs.
3. Compute and compare at least an EMA and a Holt forecast, preserving
   unavailable variance and actual model error conventions.
4. Evaluate candidate models against chronological selection/holdout fixtures.
5. Optimize a price under checked floor/cap/tick/cost constraints.
6. Emit a versioned JSON decision with source and model provenance.
7. Compare its numerical results and failure behavior against direct C MuLIB
   and the corresponding Shylock fixture workflow.

If JSON, slices, records, or output-param imports are not yet admitted, keep
this test as an explicit failing/not-yet-admitted release gate. Do not replace
it with a C program, faked result, or a manually bridged `shy_*` helper and
claim Shylock readiness.

## 10. Work packages and required sequencing

### Package A — baseline and architecture

Inventory the exact MuLIB functions/header prototypes and their dependency
closures. Identify which ExoML obligations can already be enforced and which
must be added to On1x. Commit the baseline test report, source SHA, target
triple and capability table. No speculative certificates.

### Package B — intrinsic syntax and resolution

Implement parser/AST/loader for `import kernel` and explicit unsafe context.
Make imports resolve to source-pinned Exotract metadata. Test syntax, hygiene,
unknown symbols, error messages and build-time source identity. This package
may provide discoverable-but-unadmitted modules without pretending they work.

### Package C — executable scalar functions

Implement a narrow, genuinely checked end-to-end import of a known scalar C
function using an existing admitted scalar contract profile: real CDecl,
ExoML gate, typed namespace, generated adapter, one-TU archive, real execution.
Use an existing canonical MuLIB scalar API; do not introduce a fake kernel.

### Package D — money, arrays, out records and numerical kernels

Add checked numeric spans, input length pairs, validated enum and output
record lowering, error/initialization semantics. Admit the first `statsprice`
and `histprice` functions against reviewed evidence; then `priceopt`/demand/
ensemble APIs. Deliver the forecasting/optimization demonstration. This is the
first important Shylock migration blocker.

### Package E — owned foreign handles and cleanup

Implement opaque-owned foreign resources and lifetime tracking. Begin with
KVDB; subsequently Mune and browser session handles. Prove destructor and
error-return behavior, including failure injection. Do not mark general
foreign memory or thread-affine resources supported until each model is tested.

### Package F — callbacks, transport and plugins

Implement safe synchronous callback paths where necessary, then retained
callbacks only with verified registration lifetimes and thread constraints.
Provide generic pull/event-buffer APIs for DOM and browser where simpler.
Integrate secure transport and capability-enforced Mune extensions. Do not
block the early numerical release on this broader work.

### Package G — release integration and migration

Integrate locked imports into installation, library search, source discovery,
`On1xfile` and documented build commands. Remove *application-authored* C
bridges in the Shylock proof only after parity tests pass. Update compatibility
and status docs with exact measured results.

## 11. Definition of done

The **initial KernelBind milestone** is complete only when all of these are true:

- A real On1x program uses `import kernel` and calls a canonical MuLIB function
  through a correctly generated and admitted interface, with no app-authored
  `extern fn` or native bridge.
- ExoLib/Exotract/ExoML provenance is preserved, verified and locked; the
  compiler rejects missing/unsafe/unrepresentable APIs in safe contexts.
- The program compiles to C11 and links a real exactly-once MuLIB kernel
  implementation plan, with expected behavior under strict compilation and
  negative tests.
- The ordinary On1x language and test suite continue to work.
- Evidence is reproducible and includes tested revisions, commands, target
  compiler/ABI, safety-lowering scope and remaining gaps.

**Shylock-ready KernelBind** additionally requires the complete pricing
vertical slice in §9.5, including real numerical and JSON equivalence, failure
handling, owned resources as needed and no Shylock-specific native adapters.
Do **not** conflate those two milestones.

## 12. Deliverables to check in

1. Working compiler/parser/type/safety/backend/build-tool changes and any
   necessary coordinated LibStone Exotract binding updates.
2. Kernel-specific *versioned* ExoLib profiles and ExoML contract references
   using real current MuLIB headers and canonical schemas.
3. Generated-interface format/schema, reproducible plan/admission lock and
   a read-only inspection command (such as a supported `--explain-kernel-import`
   option; choose a nonconflicting CLI spelling).
4. Executable positive, negative, differential, fuzz/bounds where appropriate,
   and ASan/UBSan tests. Include at least one documented end-to-end command.
5. Working On1x example showing an actual kernel import and call; later the
   Shylock pricing fixture.
6. `KERNEL_DIRECT_USAGE.md` updated to reflect **implemented**, **partial**,
   **unsupported** and **verified** features, with exact source paths and logs;
   an `AGENTS.md` summary pointing to this specification if appropriate.

For every work package report: exact files changed, the source SHA and target,
commands run, passed/failed/skipped results, *which concrete APIs are really
usable*, remaining blockers, and any intentionally unsafe trust boundary.
Do not replace the implementation with a series of TODO stubs, only a proposal,
or a nominally safe wrapper whose body simply calls a raw `extern fn` without
checked contract admission.

---

## 13. First demonstrator (target user-facing experience)

The following is the **design target**. The compiler must either run an
actually admitted equivalent or emit a precise `not-yet-admitted` diagnostic;
it must **not** silently compile a guessed ABI. This sample intentionally
uses high-level future API names; generated names must be documented by the
final binder.

```on1x
import kernel mulib.statsprice as Stats;
import kernel mulib.histprice as History;
import kernel mulib.priceopt as PriceOpt;

# Provided by the future checked host/data library:
# prices : Span<f64>
# model  : PriceOpt.DemandModel
# limits : PriceOpt.Constraints

fn forecast_and_optimize(prices, model, limits) {
    let market = Stats.estimate(prices, Stats.Method.Median)?;
    let prediction = History.holt(prices, 0.4, 0.2, 0.9, 3)?;
    let optimal = PriceOpt.optimize(model, limits)?;
    return (market, prediction, optimal);
}
```

Implement the smallest truthful compiler/runtime subset that makes the direct
kernel-import model real; then expand it toward this ergonomic surface and
Shylock's complete On1x-only implementation.
