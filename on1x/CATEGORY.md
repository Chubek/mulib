# CATEGORY.md — On1x Category System as Native, Self-Hosted PPX Extensions

> **Repository location:** `mulib/on1x/CATEGORY.md`  
> **Audience:** coding agents working on On1x and its self-hosted standard PPX toolchain  
> **Mandate:** implement protocols, typed witnesses, typeclass-style constraints, associated types, module signatures, ML-style functors, and related abstractions **primarily as native PPX processors written in On1x**, built in `on1x/stdppx/native/` and implemented using **expanded On1x source libraries in `on1x/stdlib/ppx/`**. Activate them with the compiler CLI or source directives such as `@enable Protocols`.  
> **Status:** IMPLEMENTATION SPECIFICATION, **not a claim that any of these constructs currently execute**. All extended syntax in examples is proposed.  
> **Baseline inspected:** `Chubek/mulib` `master` at `eeb6de585303e1ea0dfd8bf2260328289b04587c` (2026-11-17). On each agent run inspect the *actual* checkout and report its revision; do not assume this snapshot remains current.

## 0. The decisive architectural change

**Do NOT implement Category as a new set of directly parsed language keywords, nor write Category processors in Python or C++.** The earlier proposal for native `protocol`, `impl`, `witness`, `signature` and `functor` compiler constructs is replaced with an **opt-in, PPX-first architecture**:

1. `on1xc` recognizes trusted, declarative **feature activation** at the CLI or by an exact source directive (`@enable Protocols`). It resolves the corresponding **bundled, installed, native On1x PPX executables**, without relying on external Python PPXs and without conferring permission to run arbitrary source-registered programs.
2. When enabled, the front end recognizes narrowly delimited **extension syntax carriers** for module declarations, annotated functions, type constraints, and applications. It preserves their source bytes/tokens, locations, macro contexts, and module origin without assigning them semantic meaning.
3. **On1x-authored processors** under `on1x/stdppx/native/` implement parsing of Category extension syntax, protocol/witness declarations, generic constraint collection, coherence planning, specialization planning, functor application, derived witnesses, law-test generation, and lowering to ordinary checked On1x constructs.
4. Shared **On1x-authored `on1x/stdlib/ppx/category/`** libraries provide AST builders, token readers, well-formedness validators, symbol tables, canonical IDs, obligation solvers, hygienic expansion, stable metadata, resource budgets, and diagnostics. Expand existing `stdlib/ppx` facilities; do not fork its S-expression or visitor implementation.
5. The **trusted compiler host** must still type-check expanded code, validate stable module/type identities, check evidence/witness types and ownership, reject invalid or conflicting exported evidence, and preserve the proven C11 semantics. Add **small, generic, audited compiler hooks** only when genuinely necessary for soundness (e.g. typed metadata/query phases, obligation verification). PPX-produced assertions are *not* proofs by themselves.
6. All processor executables are compiled from `.on1x` to C11 using the existing On1x bootstrap backend and linked against the On1x runtime. They are not interpreted by a Python driver. The ordinary C11 backend remains the correctness reference.

**Ownership of implementation:** Category syntax and elaboration algorithms belong to On1x libraries/PPXs; generic compilation authority, evidence admission, trusted type/ownership checking, and final code emission remain with `on1xc`. **Never replace a missing type-checking step with an unchecked string-to-code rewrite.**

### Non-negotiable rules

- Retain existing source syntax, `forall` and value-restricted polymorphism, evaluation order, hygienic macros, staging, external PPXs, imports, diagnostic locations, and C11 behavior.
- Preserve the existing `--ppx LABEL PROGRAM`, `--ppx-schema`, `--stdppx`, `@ppx`, `--allow-source-ppx`, `ON1X_PPX_PROTOCOL=1`, native PPX binary protocol, and ordinary extension-carrier behavior. New activation is additive, not a replacement.
- Every new bundled Category PPX is authored in `.on1x` and imports the canonical `stdlib/ppx` libraries. C/C++ code may provide only the existing compiler/runtime transport, generic checked metadata facilities and required host capabilities; it must not become the real implementation of Category semantics behind a decorative On1x wrapper.
- No electronics-specific algorithms or kernels are within this file's scope. This is a **general-purpose language feature**.
- No mandatory third-party runtime library. Reuse the existing On1x runtime and canonical MuLIB facilities only where appropriate; preserve MuLIB's one-implementation-TU rule.
- Preserve the source-reported limitations of On1x: the current PPX sees the **untyped, macro-expanded** AST, generic record/f64/foreign ABI support may still be restricted, and native functors/traits are not yet implemented.
- All claims require actual On1x-to-C11 compile/run tests, negative tests, deterministic metadata, and evidence of type checks. Fail closed on unsupported constructs and build profiles.

### Read this implementation before touching code

- `on1x/docs/{metaprogramming,architecture,modules,polymorphism,language,roadmap,backend-mir,type-safety,staging}.md`.
- `on1x/stdppx/{README.md,native/,tests/,examples/,make_catalog.py}` and every current native processor, especially `native/{identity,derive,common}.on1x`.
- `on1x/stdlib/{ppx/,visitor/,sexpr/}`. Specifically `ppx/ppx.on1x`, `ppx/schema.on1x`, `ppx/signature.on1x`.
- `on1x/grammar/{PPX.schema.json,PPX.asdl,On1x.g,On1x.asdl}` and generated/fallback scanner parity tests.
- `on1x/src/{cli,lexer,parser,loader,macros,ppx,ppx_schema,ppx_ast,infer,sema,codegen}.cpp`, `on1x/include/on1x/{ast,type,infer,loader}.hpp`, and `on1x/src/backend/`.
- `on1x/tools/on1x-build.py`, `on1x/CMakeLists.txt`, `on1x/tests/{test_ppx.py,test_stdppx.py,test_modules.py,test_inference.py,test_poly.py}`, and `on1x/VERIFICATION.md`.
- `on1x/stdlib/catalog/functors/` is currently a **specification inventory**, not a working functor implementation. Do not promote statuses without real tests.

## 1. Feature activation: `@enable`, CLI, and provenance

### 1.1 Required source syntax

Support directives at the top level of an On1x source module, outside functions, macros and quoted values:

```on1x
@enable Protocols

protocol Eq<Self> {
    fn eq(a: Self, b: Self) -> bool;
}

impl Eq for i64 {
    fn eq(a: i64, b: i64) -> bool { return a == b; }
}
```

This is **new** source syntax. `@enable` is a compiler-recognized *activation directive* only; it does not define a general new PPX programming language in the core compiler. It is never emitted as a runtime function, variable or expression. Support optional trailing `;` consistently if it does not introduce ambiguity; the exact no-semicolon example above is mandatory.

The directive may appear more than once with distinct features; deduplicate exact repeats, but reject duplicate conflicting settings. Require it at module scope and before the first Category-dependent extension carrier/declaration in that module. Preserve its original file, line, column, and byte range. Ordinary module imports may precede it. Reject the directive inside expressions, strings, comments, macros and quoted code as an activation attempt.

### 1.2 Required compiler CLI

```sh
on1xc --enable Protocols --emit-c examples/category/eq.on1x -o eq.c
on1xc --enable Protocols --enable Functors --emit-c examples/category/sets.on1x -o sets.c
on1xc --enable Protocols,Functors --check examples/category/library.on1x
on1xc --list-extensions
on1xc --explain-extension Protocols
```

`--enable NAME` is repeatable and accepts an ordered comma-separated list, normalized to canonical registered names. `--list-extensions` and `--explain-extension NAME` are **proposed CLI additions** and must accurately report bundled availability, binary path/version, dependencies, active platform, and whether execution is permitted. For unsupported flags or unavailable installed binaries, fail with a located/CLI diagnostic; never silently continue as though the feature were enabled.

Support `--disable NAME` only if the rules are fully specified: a disabled feature must not be silently reactivated by an imported directive, alias or transitive dependency. If an enabled dependency requires a disabled feature, diagnose that conflict. `--enable` names are **not arbitrary executable paths**.

### 1.3 Feature registry and aliases

Implement a **versioned, checked-in feature registry** in the On1x installation; its schema/paths should be audited and deterministic. At minimum:

| User-facing feature | Native PPX | Description | Dependencies |
|---|---|---|---|
| `Protocols` | `on1x-ppx-protocols` | Nominal protocols, canonical and named witnesses, constrained generic calls | Core Category authoring libraries |
| `Functors` | `on1x-ppx-functors` | Module signatures, parameterized modules, sealing, module application | Shared Category identity/metadata libraries; `Protocols` only for protocol-bearing signatures |
| `Derive` | `on1x-ppx-category-derive` | Conservative, explicit witness derivation | `Protocols` |
| `Laws` | `on1x-ppx-category-laws` | Test extraction/generation and optional proof obligations | `Protocols` |
| `AutoProtocols` | `on1x-ppx-category-auto` | Trusted auto-witness requests for ownership/concurrency properties | `Protocols` plus host ownership verification |
| `DynamicProtocols` | `on1x-ppx-category-dyn` | Dynamic protocol objects and safe existential packaging | `Protocols`, runtime/resource support |
| `HigherKinds` | `on1x-ppx-category-kinds` | Kind-checked higher-kinded parametric abstraction | `Protocols`, generic type-constructor support |

Treat `Typeclasses`, `Traits`, `Witnesses` and `Category` as **documented aliases or umbrella selections** if desired, not competing semantics/registries. Suggested canonical policy: `Typeclasses` and `Traits` map to `Protocols`; `Category` expands to `Protocols,Functors` only when requested. `@enable Protocols` must be enough for the first working end-to-end acceptance fixture.

Feature names are case-sensitive canonical identifiers for initial implementation. An unknown name (or misspelling) errors, including in unused imported source.

### 1.4 Activation security, scope and imports

**Bundled system PPXs are different from arbitrary source PPXs.** An explicit `@enable Protocols` may authorize only the known compiler-installed standard Category processor whose path/version/digest are resolved from the trusted registry. It must **not** be equivalent to `--allow-source-ppx`. Preserve the latter requirement for `@ppx` registrations that execute arbitrary user programs.

Define the two accepted policies, choose one, document and test it:

- **Recommended:** each imported module has independent source `@enable` declarations; CLI `--enable` applies to the entire compilation unit. An import's directive authorizes **only that module's feature syntax**, not other importers' source text. The flattened unit retains module provenance for all Category declarations.
- Alternative: top-level `@enable` applies to the entire compilation unit, but only if clearly documented and deterministic across imports. Prefer module-local for safer builds.

An imported protocol's exported API and witness manifest must be usable by clients without executing the defining module's PPX again *unless the build deliberately recompiles that module*. Importing a module that uses Category should not implicitly enable Category syntax for other modules. Cross-module coherence must account for all relevant exported witnesses, not merely the files whose extension is syntactically enabled.

Pin the absolute standard-executable location, processor version, optional binary digest, schemas, source module inputs and compiler target in the build cache/fingerprint. Avoid PATH searching or importing executable programs from untrusted current-working directories. Source-level authorization is not a sandbox: trusted PPX code must still have bounded budgets and no unnecessary ambient I/O.

### 1.5 CLI, directives, and ordinary PPXs must coexist

`@enable Protocols` is **not** `@ppx Protocols PATH` and is **not** `@Protocols(...)`. It opts into a feature profile. Existing `@label("name", ...)` carriers and standard PPXs continue to work unchanged.

Define a stable processing order: preflight feature collection → import graph/module provenance → macro expansion → Category syntax/metadata PPX phases → ordinary compatible PPXs (or an explicitly documented phase split) → typed evidence validation → C11. Prohibit arbitrary PPX reordering that changes witness selection, and reject conflicting registrations with the same label or extension schema owner.

Tests must show CLI/directive parity, imports with differing enabled features, no environmental hijacking, working installed binaries, and deterministic disabled-feature errors.

## 2. Honest architectural boundary: what a PPX can and cannot do

The current standard PPX API receives a **versioned S-expression representation of the untyped, macro-expanded AST**, prior to inference. It does not currently receive typed HIR, normalized type equalities, resource information, or module exports. Its `Unit` schema currently carries ordinary functions/externs, not arbitrary top-level declarations. A PPX cannot soundly implement implicit typeclass search or opaque module abstraction by string substitution alone.

Therefore implement Category in **two bounded stages**, with explicit host assistance:

**Stage A, syntax/expansion (On1x PPX):** parse Category carriers; construct declarations, unique identifiers, constraints and witness records; do source-level duplicate/orphan/overlap checks; lower directly executable restricted features to core On1x functions; produce a versioned **Category request/evidence manifest**.

**Stage B, typed elaboration (native On1x PPX plus generic checked compiler broker):** after the compiler determines inferred/concrete types, expose a minimal typed query/result protocol; the On1x PPX resolves normalized obligations and plans witness insertion/specialization; the **compiler independently verifies** the candidate evidence against actual inferred types, module identity, visibility, effects/ownership and ABI rules before emitting executable code.

This is an opt-in **PPX extension of the compiler pipeline**, not an ordinary v1 tree rewrite masquerading as a type checker. Avoid giving PPX an unchecked ability to inject already-validated HIR, constructor proofs, privileged intrinsics or arbitrary function exports.

**If typed facts and a trusted verifier do not yet exist, restrict initial release to explicit, concrete witness use, and reject generic implicit resolution.** P0 tests must exercise real code; never label a parsed but ignored protocol constraint as implemented.

## 3. Extension-carrier design and surface syntax

### 3.1 Host syntax gateway; Category parses the language

The core On1x scanner can already tokenize `@`, identifiers, braces and most other punctuation, but the current source parser cannot accept arbitrary top-level `protocol`/`impl`/`signature` declarations or Rust-style `&T` and generic bounds. The existing `@label(...)` carrier is expression-only (plus function attributes). Implement a **small generic module-declaration extension gateway**, not dedicated protocol grammar in C++.

Recommended gateway forms (select and document one canonical representation):

```on1x
@enable Protocols

@extension Protocols {
    protocol Eq<Self> { fn eq(a: Self, b: Self) -> bool; }
    impl Eq for i64 { fn eq(a: i64, b: i64) -> bool { return a == b; } }
}
```

The gateway token collector stores a **balanced raw token/source slice** with its location, lexical context and feature owner. A `Protocols` processor written in On1x parses the inner Category grammar. This is the **mandatory bootstrap syntax** if direct declarations are not immediately feasible. Do not require string-encoded source or unsafe arbitrary code evaluation.

Add ergonomic direct syntax only as **opt-in contextual sugar**:

```on1x
@enable Protocols
protocol Eq<Self> { fn eq(a: Self, b: Self) -> bool; }
impl Eq for i64 { fn eq(a: i64, b: i64) -> bool { return a == b; } }
```

The source gateway recognizes that this declaration belongs to a registered enabled feature and packages a token span for its On1x parser. The core C++ parser **must not decide protocol syntax validity, coherence or witness semantics**. Unknown feature keywords outside an enabled profile remain rejected as ordinary invalid On1x source. The gateway must preserve ordinary `protocol`/`impl` identifiers where context permits and not break macro/PPX expansion.

To support type parameter constraints (`fn min<T: Ord>(...)`) when the current core parser does not accept them, package the *entire constrained declaration* as a Category carrier, or extend the **generic** token-span carrier. Similarly, functor applications and explicit `using` evidence arguments may be routed via typed extension carriers. Never partially parse and silently drop generic annotations.

### 3.2 Required schema additions

Extend `grammar/PPX.schema.json` (and any ASDL/fallback transports as appropriate) **without changing existing v1 meanings**. New owner-namespaced proposal tags:

```json
{
  "version": 1,
  "nodes": {
    "Protocols:Decl": {
      "category": "Syntax",
      "fields": [["tokens", "Bytes"], ["location", "Location"]]
    },
    "Functors:Decl": {
      "category": "Syntax",
      "fields": [["tokens", "Bytes"], ["location", "Location"]]
    }
  }
}
```

This fragment is **illustrative only**: actual versioned schemas must match the host's validated categories and field/list conventions. The existing root `Unit` currently has only `Function*` elements; to insert a declaration there, add a compatible `ModuleItem` category/root v2 envelope, or an explicit parallel declaration collection, **not a forged Function**. Do not redefine `Unit`, silently reinterpret existing fields, or allow unregistered tags. Preserve original source spans rather than relying on synthetic filenames.

The host validates that a carrier is owned by an enabled processor and that its declared field types and resource budgets match the schema. After expansion, no undecoded declaration or unconsumed expression carrier may reach ordinary HIR checking.

### 3.3 Category syntax to parse inside native PPX

The On1x PPX grammar should accept, under the relevant enabled feature:

```on1x
protocol Eq<Self> { fn eq(a: Self, b: Self) -> bool; }
protocol Ord<Self>: Eq<Self> { fn compare(a: Self, b: Self) -> i64; }

impl Eq for i64 { fn eq(a: i64, b: i64) -> bool { return a == b; } }

witness Reverse: Ord<i64> {
    fn compare(a: i64, b: i64) -> i64 { return b - a; }
}

fn minimum<T: Ord>(a: T, b: T) -> T {
    if Ord.compare(a, b) <= 0 { return a; }
    return b;
}

fn sort<T>(values: Vec<T>, using order: Ord<T>) -> Vec<T> { /* ... */ }
let xs = sort(values, using Reverse);
```

All examples are **desired syntax**. `Vec<T>`, records, `Ordering`, named parameters, borrow types, and ADTs must only be accepted once the corresponding actual On1x types are implemented, or replaced with executable smaller examples. The source PPX is responsible for parsing these Category constructs; the host owns generic balanced-span capture and type validation of expanded code.

For the first executable subset, use `i64`, `bool`, normal functions and explicit evidence—types that already have working On1x codegen. Advanced syntax is introduced in milestones, not promised by scaffolding.

## 4. Required `on1x/stdppx/` implementation

Create native source processors; do **not** ship Python alternatives as the implementation of the new Category functionality:

```text
on1x/stdppx/
├── native/
│   ├── protocols.on1x                 # Protocol declarations and typed witnesses
│   ├── functors.on1x                  # Signatures, sealing, module functor expansion
│   ├── category_derive.on1x           # Explicit derivation
│   ├── category_laws.on1x             # Law-test/proof-obligation generation
│   ├── category_auto.on1x             # Compiler-verified auto-protocol requests
│   ├── category_dyn.on1x              # Dynamic witnesses, later milestone
│   ├── category_kinds.on1x            # Higher-kinded forms, later milestone
│   └── common.on1x                   # Reuse/extend, do not duplicate parser/visitor
├── category/
│   ├── features.json                  # Trusted, versioned feature registry
│   ├── protocols.schema.json
│   ├── functors.schema.json
│   └── README.md
├── examples/category/
│   ├── eq.on1x
│   ├── ordering.on1x
│   ├── associated.on1x
│   ├── functors.on1x
│   ├── derive.on1x
│   └── concurrency.on1x
└── tests/
    ├── test_category_activation.py
    ├── test_category_protocols.py
    ├── test_category_witnesses.py
    ├── test_category_functors.py
    └── test_category_security.py
```

Paths are suggestions; prefer actual layout conventions from the checked-out repository. The Python files above are **test drivers only**. Actual protocol/functor logic is in native `.on1x` modules.

The processor binary names installed by CMake are **exactly** documented and discoverable by `--explain-extension`: `on1x-ppx-protocols`, `on1x-ppx-functors`, etc. Avoid requiring a dev checkout or Python at PPX execution time. Keep cross-platform behavior fail-closed when an executable cannot run; do not silently use a Python approximation.

Each PPX:

- Imports the shared `stdlib/ppx/category/` support, and invokes the standard validated `ppx_serve` transport or a backwards-compatible v2 typed mode.
- Has deterministic bounded work, expansion depth and memory-use rules; propagates source spans and macro provenance.
- Emits exactly one well-formed message for the selected protocol version, or a nonzero diagnostic; no logs on stdout.
- Rejects unknown AST constructors, malformed packets, unsupported versions, duplicate declarations, invalid ownership/visibility and contradictory constraints.
- Generates ordinary On1x functions, stable evidence descriptors and typed requests, not unchecked native/C fragments or hidden FFI calls.
- Does not execute Category user code during compilation. Any constant evaluation is tightly bounded and restricted to a pure, verified subset.
- Preserves original compilation-unit state if it fails; no partial generated code or cache entries are published.

## 5. Expand `on1x/stdlib/ppx/` in On1x itself

The existing `on1x/stdlib/ppx` includes `ppx.on1x`, `schema.on1x`, signature helpers and a canonical S-expression/visitor base. **Extend these rather than reimplement them in C++ or embedding a second Python JSON/AST engine.** Proposed addition:

```text
on1x/stdlib/ppx/
├── ppx.on1x                    # Existing: keep compatible; extend APIs
├── schema.on1x                 # Existing: schema validation
├── signature.on1x              # Existing: type printing, extend to Category views
├── ast.on1x                    # Generic constructors + typed category checks
├── tokens.on1x                 # Bounded token-span reader + locations
├── source.on1x                 # Span/provenance and hygienic name creation
├── diagnostic.on1x             # Structured, stable diagnostic results
├── manifest.on1x               # Versioned sideband import/export of facts
├── registry.on1x               # Feature/processor resolution metadata
├── typed.on1x                  # Read-only compiler typed-fact queries
├── expansion.on1x              # Deterministic rewrites and collision checks
└── category/
    ├── README.md
    ├── syntax.on1x             # Category grammar over extension spans
    ├── model.on1x              # Protocols, types, signatures, functors, witnesses
    ├── names.on1x              # Stable identity, hygienic names and namespaces
    ├── kinds.on1x              # Kind checking, when supported
    ├── obligations.on1x        # Canonical predicates and associated equalities
    ├── solver.on1x             # Bounded memoized evidence resolution
    ├── coherence.on1x          # Orphan, overlap, cycle and conflict checks
    ├── witness.on1x            # Typechecked evidence planning and upcasts
    ├── projections.on1x        # Associated-type normalization
    ├── dispatch.on1x           # Static calls and explicit dictionary plans
    ├── functors.on1x           # Signature matching, applicativity/generativity
    ├── derive.on1x             # Hygienic conservative derivation
    ├── laws.on1x               # Property tests and explicit proof status
    ├── effects.on1x            # Requested effect/ownership obligations
    ├── report.on1x             # Obligation explanations/diagnostic traces
    └── serialize.on1x          # Canonical metadata encoding / fingerprint
```

### Required authoring-library APIs

Implement in actual `.on1x` code, with unit tests run through the real compiler:

- `ppx_read_tokens`, `ppx_peek`, `ppx_expect`, `ppx_parse_delimited`, `ppx_source_location`, `ppx_location_merge` with checked bounds and length-aware bytes.
- `ppx_category_parse_protocol`, `ppx_category_parse_impl`, `ppx_category_parse_witness`, `ppx_category_parse_signature`, `ppx_category_parse_functor`, `ppx_category_parse_where`.
- `ppx_category_collect_declarations`, `ppx_category_qualify_name`, `ppx_category_scope`, `ppx_category_lookup_export`.
- `ppx_category_make_obligation`, `ppx_category_normalize`, `ppx_category_unify`, `ppx_category_resolve`, `ppx_category_explain`.
- `ppx_category_check_coherence`, `ppx_category_check_orphan`, `ppx_category_check_overlap`, `ppx_category_check_termination`.
- `ppx_category_emit_witness`, `ppx_category_lower_call`, `ppx_category_specialize`, `ppx_category_emit_functor_module`.
- `ppx_category_read_typed_facts`, `ppx_category_make_evidence_plan`, `ppx_category_emit_metadata` where typed mode has been implemented and **host validation is compulsory**.

Function spellings may be adapted to established On1x style, but document the actual public APIs. Each API must be bounds-checked, type-correct On1x source, and independent of interpreter-only facilities. Do not mark a named API implemented if only a stub or catalog entry exists.

### Data structure choice in the bootstrap phase

The current On1x PPX authoring library uses `sexpr` and `buffer` and does not yet have complete user records, ADTs, or generic arrays. Initial Category parser/solver types may use **tagged, schema-validated S-expression records** as an internal PPX representation, provided fields are validated on every boundary and the module APIs hide raw representation. Upgrade to richer native On1x records/ADTs when available. This is a *compile-time internal representation only*, **not** an unsafe substitute for user-facing typed witnesses.

Use canonical, deterministic ordering and explicit step/node/string budgets. Do not let shared S-expression nodes create exponential expansion or pointer-identity-based cache keys.

## 6. PPX wire protocol and typed feedback loop

### 6.1 Preserve ordinary v1 transformers

Ordinary PPXs today consume one canonical S-expression on stdin and emit one S-expression AST on stdout. Their schema-v1 contract (`ON1X_PPX_PROTOCOL=1`, `ON1X_PPX_LABEL`) must remain valid.

Category's v1-compatible **concrete evidence subset** can generate only core On1x functions and calls and rely on normal post-PPX checking. Do not introduce privileged metadata until needed.

### 6.2 Introduce optional typed Category request protocol

For advanced constrained polymorphism, provide a **versioned, opt-in typed extension** that supports a request/response phase *after inferred types are available*. Do not modify v1's message format in place. A suggested protocol is `on1x.category-ppx/v1`, with a trusted host broker that manages:

1. **Syntax phase:** send schema-checked syntax spans and module provenance; receive lowered/annotated core candidate AST plus **declaration and obligation requests**.
2. **Inference phase:** run the ordinary On1x checker on the candidate, recording concrete type identities, generic binder identities, visibility, source spans and specialization keys.
3. **Evidence phase:** send *read-only canonical type facts and normalized outstanding obligations* to the native Category PPX. It returns a **proposed evidence plan** (witness IDs, substitutions, resolved associated equalities, callsite plans) without the ability to modify prior type facts.
4. **Admission phase:** the host checks each plan against the canonical declared/checked implementations, inferred target types, constraints, imports and effects; inserts required typed evidence arguments; rechecks the fully elaborated candidate before code emission.
5. **Publication:** atomically commit verified HIR/module interfaces and artifact hashes. Unknown queries, ambiguous solutions, cycles, wrong fingerprints or expired budgets abort without output.

This is a design requirement, **not a description of an existing `on1xc` API**. The agent may choose an equivalent protocol if it is smaller and proves the same safety/semantic properties. The type environment in the host is authoritative. The PPX must never fabricate a compiler proof token or bypass checking via a privileged AST constructor.

**No arbitrary host callbacks from the PPX** are required for the first implementation: use a bounded multi-round subprocess exchange or multiple deterministic invocations with validated immutable fact snapshots. Record version, target triple, compiler revision, feature registry digest, source digest, normalized type environment identity and round number.

### 6.3 Cross-module compilation

Maintain deterministic export/interface metadata for compiled Category-bearing modules. It should include protocol IDs, witness IDs and types, associated constraints, signature IDs, visibility, functor application identity and source fingerprints. Verify cache imports against the exact compiled interface, target and feature profiles.

A PPX cannot privately declare a global canonical witness that conflicts with one exported by a dependency. A trusted **coherence admission** step (host broker + On1x Category algorithm) must check all visible canonical witnesses before emitting code. Imported names may be renamed, but the identity of the defining declaration must not change.

## 7. Protocols: normative semantics

A **protocol** is a nominal, package-qualified interface over type parameters; it is not auto-satisfied by a structurally matching set of functions. Required members may include function signatures, associated types/constants and inherited protocol predicates. Default functions are permitted only after their own bodies have passed normal checking under all required predicates.

Minimal desired forms:

```on1x
@enable Protocols
protocol Eq<Self> { fn eq(a: Self, b: Self) -> bool; }
protocol Ord<Self>: Eq<Self> { fn compare(a: Self, b: Self) -> i64; }
```

At least P0 implementation must make a concrete protocol declaration bind a stable protocol identity, check member signatures, check an implementation body, and call that member through an explicitly checked witness. Only later permit constrained generic methods.

- `Self` is a bound identity, never an implicitly global type. Protocol parameter order is stable and part of identity.
- Inheritance `Ord<T> => Eq<T>` generates a required predicate, **not nominal class inheritance or automatic memory-layout inheritance**.
- Methods are explicit functions; special receiver syntax is optional sugar, not a hidden new ABI.
- Methods with effects or resource operations must obey the ordinary On1x checker. Foreign/raw functions are not implicitly safe merely because they inhabit a protocol.
- There is no unrestricted implicit method lookup by name. A call references a specific protocol member or an unambiguously resolved witness.

## 8. Witnesses: canonical, named, and explicit evidence

### 8.1 Canonical implementations

```on1x
impl Eq for i64 {
    fn eq(a: i64, b: i64) -> bool { return a == b; }
}
```

A canonical `impl P for T` is a candidate for automatic evidence resolution. It must check method types, associated equalities, inherited requirements, visibility, orphan eligibility, overlap and dependency termination. More than one applicable canonical implementation is an error irrespective of source/import order.

### 8.2 Named alternatives

```on1x
witness Ascending: Ord<i64> {
    fn compare(a: i64, b: i64) -> i64 { return a - b; }
}

witness Descending: Ord<i64> {
    fn compare(a: i64, b: i64) -> i64 { return b - a; }
}

let first = minimum(7, 3, using Ascending);
```

This is conceptual syntax: signed subtraction can overflow; real ordering tests should use a checked comparison primitive. Named witnesses are **not** automatically inserted into global canonical resolution. Multiple named alternatives can coexist and are intentionally chosen by explicit `using` evidence. Their member functions must still be type-checked.

For initial executable tests, generate fresh helper functions and explicit named evidence selectors using only existing On1x scalar/function/tuple types. Expand to full witnessed generic functions only after the typed feedback loop is implemented.

### 8.3 Runtime evidence representation

Elaboration makes evidence explicit. Where a function is monomorphized and witness identity known, emit a statically resolved **direct call**. For generic code that cannot be fully specialized, pass a validated evidence dictionary/function record with an exact compiler-known layout and ownership protocol. Do not encode arbitrary method pointers in unchecked `i64` values or expose caller-forged `sexpr` dictionaries.

Dynamic dispatch (`dyn P`) is deferred until existential object layout, drop functions, lifetime rules, vtable ABI, object safety and effect/ownership checks exist. A dynamic witness must not fake static dispatch or silently discard type information.

## 9. Coherence, orphan rules and deterministic resolution

The On1x PPX owns most of the coherence/resolution algorithm, while the compiler host is the **trusted admission authority** that validates the resolved dependencies and prevents cross-module conflicts.

**Default policy:** a canonical witness for `P<T...>` may be declared only in a package defining `P` or an eligible local type constructor. Initially reject blanket implementations for a foreign protocol applied solely to foreign types; do not use file import order as an ownership escape hatch. Check every instantiated head for overlap after type substitution, not just raw textual comparison.

Resolve obligations in this order:

1. Explicit `using W` evidence supplied at the callsite.
2. Explicit evidence parameter lexically available in the current generic scope.
3. A unique admissible canonical witness from the current verified module/dependency world.
4. No candidate → error; multiple candidates → ambiguity error.

Do **not** implement implicit fallbacks to the first import, last definition, lexical name match, hash order or a local arbitrary witness. No ad-hoc priority numbers in v1.

Memoize by `(predicate, normalized type tuple, relevant evidence scope, world revision, feature version)`; detect repeated goals/cycles and enforce a decreasing well-founded measure on recursive instance heads. If a valid acyclic proof exceeds a budget, report exhaustion/unknown, not success or unsatisfiable.

Negative tests: duplicate canonical `Eq<i64>` in two modules; conflicting blanket heads; external orphan; imported alias changing identity; ambiguous witness; recursive `P<T> where P<T>`; leaking named local evidence into unrelated scopes; conflicting associated-type equations.

## 10. Constrained polymorphism and type inference

Keep On1x's existing predicative `forall`, value restriction, specialized generic functions, and ordinary structural unification. Add Category predicates to qualified schemes, e.g.

```text
forall T. Ord<T> => (T, T) -> T
```

A generic body is typechecked **once under a valid protocol dictionary assumption**; calls create obligations for the instantiated type arguments. Weak inference variables are not generalized merely because a witness happens to exist. Ordinary unification and rigid binders remain authoritative. Do not implement two incompatible polymorphism systems.

Native Category PPX responsibilities: parse and normalize protocol predicates, collect generic constrained declarations, map concrete type demands to witness requests, create deterministic specializations/call rewrites and submit them for host type validation.

Trusted host responsibilities: store qualified type schemes (or verifiably equivalent hidden explicit evidence parameters), check member signatures and generic body uses, infer concrete caller types, verify selected evidence and recheck generated calls. If the current host representation cannot preserve a generic constraint safely, reject the generic declaration until it can.

New explicit helper APIs should report obligation traces and canonical source positions; no hard-to-reproduce inference based on PPX execution order.

## 11. Associated types, constants and higher kinds

Desired syntax:

```on1x
protocol Iterator<Self> {
    type Item;
    fn next(self: Self) -> Option<Self::Item>;
}

protocol Convert<Source, Target> {
    fn convert(value: Source) -> Target;
}

protocol FixedSize<Self> {
    const BITS: usize;
}
```

`Option`, user-defined records and `usize` shown here are **not assertions about the current On1x type system**. The first executable associated-type tests must use actually supported concrete types, or explicitly depend on tested prerequisites.

Represent associated types as projections `(protocol ID, ordered type arguments, associated item ID)`, with normalized equality constraints. The Category PPX resolves declarations and computes normalization *only from checked, coherent evidence*; the host validates substitution against actual type environment. Detect cycles, ambiguous projections, kind mismatches and missing equations.

Multi-parameter protocol coherence uses the entire ordered type tuple, not a special `Self` head. Associated constants must be pure, bounded compile-time values of checked types; no arbitrary compile-time user code execution. Higher-kinded types and type-level functions are **later milestones**: introduce proper kind checking and well-founded normalization before accepting them, not an unrestricted general theorem prover hidden in the PPX.

## 12. Module signatures and ML-style functors as On1x PPXs

Functors are **not** synonyms for typeclasses. A module signature describes types and exports; a functor transforms statically typed modules into a module with its own abstract type identities and export set.

Desired syntax with `@enable Functors`:

```on1x
@enable Functors
signature ORDERED {
    type Element;
    fn compare(a: Element, b: Element) -> i64;
}

signature SET {
    type Element;
    fn insert(value: Element);
    fn contains(value: Element) -> bool;
}

module functor MakeSet(O: ORDERED) -> SET
    where Element = O.Element {
    // Body contains normal On1x declarations under a checked module environment.
}
```

The native `functors.on1x` PPX and `stdlib/ppx/category/functors.on1x` handle syntax, module-parameter substitution, export selection, name mangling, signature matching, application planning, static module construction, cache keys and sealing.

**The compiler's module loader remains the source of truth** for canonical module origins/imports and ordinary export visibility. It must verify interface identity, prevent hidden type or private implementation export, and publish stable typed module interfaces. Do not represent an abstract module type as merely a C struct whose layout happens to match another.

- **Applicative by default:** equivalent functor identity + canonical module arguments + versioned interface hashes yield the same abstract type identities.
- **Generative only explicitly:** `generative functor` / `fresh` (exact syntax to be specified) produces distinct nominal type identities per specified application identity; not per nondeterministic PPX execution timestamp or pointer.
- Nested functors may be deferred until single-argument/multi-argument independent modules, import cycles and sealing are correct.
- Converting an exported module operation to a protocol witness requires an explicit checked adapter.
- Dynamic runtime modules are out of scope for initial functors.

Bootstrap with a functor that exports scalar functions under a namespaced, statically specialized module. Only mark **abstract type sealing** implemented when a negative test proves that distinct abstract identities cannot be confused.

## 13. Derivation, auto protocols, and semantic laws

### Derivation

`@enable Derive` authorizes a trusted self-hosted `category_derive.on1x` PPX to **construct checked witnesses** for well-defined shapes. Start with a precisely bounded `Eq`/`Display` profile over supported records/tuples; recursively require a witness for each field. Reject recursive unbounded shapes, inaccessible fields, hidden resources, and dynamic foreign objects. Generated names must be hygienic and stable; caller cannot inject privileged core names through textual collisions.

### Auto protocols and concurrency

`Send`, `Sync`, `Copy`, `Drop` and equivalent ownership/effect predicates may be **compiler-controlled auto protocols**, but **never grant them because the On1x PPX says a struct looks safe**. The native `category_auto.on1x` can request a derivation and provide a candidate trace; the compiler must use actual ownership, aliasing, foreign contract and thread-affinity facts to admit or reject it. Unknown foreign thread-safety is *not* `Send` by default. Only a separately verified unsafe/manual escape hatch may override, with explicit capability and diagnostics.

Support generic constraints for future `spawn`, `Task<T>`, channels and parallel workers, while integrating with the **real** On1x and LibStone Plaid concurrency roadmap. Do not misrepresent current concurrency as already lowering through Category.

### Laws and proofs

`@enable Laws` invokes native `category_laws.on1x` to parse law declarations, generate **reproducible property-based tests**, maintain a tested/not-tested status and optionally request an independent proof checker. Distinguish:

- `well_typed`: member signatures and witness implementation were compiler-checked;
- `tested`: selected generated test cases passed with recorded seed and coverage;
- `proven`: a proof certificate was validated under recorded assumptions by a named checker.

Never treat a passing finite test suite or a Boolean PPX flag as mathematical proof. The PPX may use LibStone solvers *as an optional compiler-side tool*, but generated programs must not require the SDK to run.


### Sealed protocols, defaults, and controlled specialization

Support `sealed protocol P` only after the compiler can associate a stable *defining package identity* with each protocol and witness. A sealed protocol permits canonical implementations only from the defining package or its explicitly authorized members; simply importing the package never grants implementation authority. The self-hosted `protocols.on1x` PPX checks declarations and proposes the authorization decision; the host verifies package identity before accepting it.

Default method bodies must be checked in the protocol's generic evidence environment, preserve effects, and be overridden only with a signature-compatible method. Inherited protocols bring additional obligations, not an inherited memory layout. Allow strictly controlled specialization, if ever added, **only** within sealed scopes with a deterministic partial ordering of implementation specificity; reject ties and cyclic dominance. Initially reject all overlapping canonical implementations, even if their source declares a `priority`. Named, explicit witnesses are the preferred alternative to specialization.

### Lawfulness versus interface admission

A coherent and well-typed canonical witness is *not* evidence of Eq reflexivity, Ord transitivity, `Hash` consistency, or free-threaded execution. The self-hosted laws processor may generate tests or external verification requests, but the host must not use unproven semantic laws as optimization preconditions. For optimizations requiring an algebraic law, record either a trusted primitive invariant or a separately verified proof with precise assumptions.

## 14. Dynamic dispatch, specializations, and ABI

For static calls, lower a checked witness to direct calls and allow the existing bounded specialization engine to optimize them. For non-specialized generics, use typed dictionary passing; the dictionary's ABI, lifetime, borrow rules and linkage must be explicit and target checked.

For `dyn Protocol`, require an object-safe protocol subset, type-erased payload, vtable identity and ABI version, method signatures, drop/clone rules, mutability/ownership transfer, error behavior, and lifetime checking. External/foreign callback safety remains governed by existing ExoML/Chisel/On1x limitations. Do not generate C calls by guessing function-pointer ABI or by packing pointers into unchecked integers.

Keep emitted applications C11 and selected run-time kernels. The compiler and self-hosted PPXs may use richer build-time dependencies, but an ordinary program does not link the On1x compiler or invoke the PPX at runtime.

## 15. Compiler modifications: strictly minimal, generic and auditable

Allowed compiler-side changes (with their actual source paths):

| Existing location | Required minimal change | Must NOT implement there |
|---|---|---|
| `src/cli.cpp`, build helper | Parse `--enable`, report feature registry; resolve native bundled PPXs | Protocol grammar, instance resolution |
| `src/lexer.cpp`, generated/fallback scanners | Recognize exact `@enable` directive and capture balanced extension tokens | A Rust or Haskell language frontend |
| `src/parser.cpp`, `include/on1x/ast.hpp` | Generic module-declaration carriers and provenance, only under enabled registration | Typeclass semantics, functor expansion |
| `src/loader.cpp`, `include/on1x/loader.hpp` | Module-local activation and canonical origin/identity; compiled interface metadata | Case-specific witness selection |
| `src/ppx.cpp`, `src/ppx_ast.cpp`, `src/ppx_schema.cpp` | Trusted native registry integration; compatible v1 + optional typed v2 carrier/metadata protocol | A second implementation of Category's solver |
| `src/type.cpp`, `src/infer.cpp`, `src/sema.cpp` | Small verified generic obligation/evidence admission, typed facts and qualified types *where unavoidable* | Grammar parsing, ad-hoc implicit search |
| `src/codegen.cpp`, `src/backend/` | Exact evidence argument lowering, direct calls, type/ownership checks, C11+MIR compatibility | Runtime PPX execution, unchecked native ABI |
| `CMakeLists.txt` and `tools/on1x-build.py` | Build/install/self-host native processors, cache digest, enable flags and dependencies | Python-based Category implementation |

Generic compiler hooks must not special-case `Eq`, `Ord`, `Iterator`, `Functor`, `Send`, etc. by their source names. Their job is only to preserve and verify typed facts, identities, effects and ABI. The corresponding specific semantics belong in the native PPX/On1x Category library.

If this boundary proves impossible for a proposed advanced feature, **document the minimum trusted core primitive needed** and implement that primitive generically. Never bury an unsound assumption to achieve a misleading “PPX-only” milestone.

## 16. Concrete build / install plan

Extend the existing CMake native PPX build list. Do not build generic Python implementations in place of On1x processors.

```cmake
# Illustrative, adapt to the current native PPX build mechanism:
foreach(_processor protocols functors category_derive category_laws
                   category_auto category_dyn category_kinds)
  # Compile stdppx/native/${_processor}.on1x with on1xc --emit-c,
  # link generated C11 against on1x_runtime,
  # install as libexec/on1x/on1x-ppx-${_processor},
  # record version/schema/hash in the trusted extension registry.
endforeach()
```

**Bootstrapping:** compile standard PPXs with a *minimal bootstrap profile* that does not require Category itself. Existing On1x's generated C11, `sexpr`, `buffer`, module imports and PPX libraries must be sufficient for the initial implementation. Prevent cycles in which building `on1x-ppx-protocols` requires activating `Protocols`. Add explicit CMake dependencies and one deterministic build recipe.

The existing `--stdppx NAME` currently locates **Python** files; preserve it. New `--enable NAME` must resolve **native On1x-built** executables by the trusted registry. Optionally add `--stdppx-native NAME` as a convenience, but do not quietly reinterpret existing command lines.

Installed builds must resolve executables and schemas relative to their installed prefix, not to the source checkout. Cross-compilation must distinguish build-host PPX binaries from target executable files; the PPX runs on the **build host** and emits target-independent checked metadata/code where appropriate.

Add deterministic `--emit-category-plan`, `--dump-category-obligations` or equivalent inspection commands only after their serialized format and security boundaries are specified. Inspection must not accidentally execute source PPXs without authorization.

## 17. Diagnostics and failure policy

Use stable, located diagnostic identifiers, e.g.:

| ID | Meaning |
|---|---|
| `ON1X-CAT-ENABLE` | Unknown/disabled/late/malformed `@enable` or CLI feature |
| `ON1X-CAT-PPX` | Missing/untrusted/incompatible native processor or protocol packet |
| `ON1X-CAT-SYNTAX` | Category grammar failure in a captured token span |
| `ON1X-CAT-PROTOCOL` | Invalid protocol or member declaration |
| `ON1X-CAT-WITNESS` | Invalid/missing/wrong-typed witness |
| `ON1X-CAT-COHERENCE` | Conflicting canonical implementations/orphan/overlap |
| `ON1X-CAT-OBLIGATION` | Unsolved, cyclic, ambiguous or exhausted predicate |
| `ON1X-CAT-ASSOC` | Invalid/ambiguous/circular associated-type normalization |
| `ON1X-CAT-FUNCTOR` | Signature mismatch, identity error or sealing violation |
| `ON1X-CAT-AUTO` | Unproven ownership/thread-safety capability |
| `ON1X-CAT-CACHE` | Stale, mismatched or corrupt interface/evidence manifest |

Diagnostic messages should include the originating source and macro expansion point, applicable protocol and concrete type, candidate witness list with reasons for exclusion, and bounded chain of obligations. Preserve original source positions through token carriers, metadata snapshots and typed feedback. A processor failure must not emit any final C, leave corrupted interface cache entries, or claim `verified` status.

## 18. Tests: executable proof that the feature works

All tests must compile actual `.on1x` clients with the **native, self-hosted Category PPX**, then compile the emitted C11 with GCC and Clang and run it. Python may orchestrate tests but may not implement the Category semantics being tested. Preserve preexisting PPX/macro/staging tests and ensure the generated-scanner and fallback-scanner builds behave identically.

### 18.1 Positive tests

1. `@enable Protocols` parses an actual declaration and implementation, generates checked calls and produces correct output.
2. `--enable Protocols` executes the same code without a source directive; AST/code and results are semantically identical.
3. Multiple named witnesses coexist and `using X` selects precisely X; canonical instance choice is unaffected.
4. Functions constrained by a protocol are checked once against its members, and legitimate concrete specializations resolve to correct implementations.
5. Imported canonical implementations and interface manifests have stable identity after aliasing, moving installation prefix and separate compilation.
6. Associated types normalize through checked witnesses, including nested generic use.
7. `@enable Functors` builds a concrete module, checks signatures, applies an applicative functor and enforces sealing on abstract exports.
8. Explicit generative functor produces distinct abstract types, with reproducible artifact identities in a specified build environment.
9. Derivation generates a verified witness for supported fields; law tests record deterministic seeds and do not claim proof.
10. A hypothetical `Send` capability is admitted only through the checked ownership/effect facts, not by arbitrary source PPX declaration.
11. The installed compiler runs native Category PPXs without Python and without a source checkout, using a relocated prefix.
12. Category-enabled and ordinary PPXs coexist without changing unrelated source compilation, diagnostics or runtime behavior.

### 18.2 Negative tests

- `protocol ...` without an enabled feature; unknown/misspelled feature; nested or late directive; source string/comment containing `@enable`; unsupported platform/missing executable.
- `@enable` or an alias attempting to authorize arbitrary external code; malicious path in registry; mismatched binary/schema digest; excessive wire size/processor time.
- Conflicting canonical implementations across imported modules; overlap; orphan; undecidable recursive instance; budget exhaustion; ambiguous associated projection.
- Named evidence of the wrong protocol/type; inaccessible private witness; leaked lexical witness; mismatched protocol member type/arity.
- Generic constraint erased by PPX; forged typed-fact packet; forged evidence-plan entry; wrong module interface fingerprint; stale cache; insufficient ABI.
- Functor applying a module with missing exports, wrong abstract type equations, or private type leakage; generative identity confused with applicative identity.
- `Send`/`Sync` inferred for unknown foreign handle; derived `Copy` for a unique resource; dynamic witness lifetime unsoundness.
- Bootstrap dependency cycle; native processors accidentally compiled using themselves; schema-v1 regression or original PPX labels broken.

### 18.3 Metamorphic and stress tests

Reorder imports, definitions, filesystem traversal order and hash seeds; canonical resolution must remain identical or diagnose the same conflict. Test deep type applications and obligation cycles against bounded work. Repeat identical builds and compare normalized manifests. Fuzz extension carriers/wire messages/schema parser, including invalid UTF-8 and embedded NUL at prohibited interfaces. Use ASan/UBSan for C11 output and compiler/PPX runtimes; use TSAN where threads actually exist. Test non-POSIX failure paths rather than silently substituting another executor.

### 18.4 Suggested validation commands

```sh
cmake -S on1x -B build/on1x -DBUILD_TESTING=ON \
  -DON1X_BUILD_NATIVE_PPX=ON
cmake --build build/on1x --parallel 4
ctest --test-dir build/on1x --output-on-failure
python3 -m unittest discover -s on1x/stdppx/tests -v
python3 tools/check.py
```

Paths are from MuLIB's root. Adapt `--parallel` and tool paths to the environment. Record exact commands, compiler flags, build/host target, commit hashes, failures, skips and feature-specific test cases; a check command's existence does not imply it passes.

## 19. Implementation milestones (M0–M8)

| Milestone | Deliverable | Must pass before promotion |
|---|---|---|
| **M0 — Native activation** | `@enable`, `--enable`, trusted registry, native PPX build/install, backwards-compatible v1 | Enabled/disabled/import/security/relocation tests; original PPXs pass |
| **M1 — Syntax and explicit scalar protocols** | Extension carrier, self-hosted Category grammar, concrete `Eq<i64>`, explicit witness and method dispatch | Actual On1x → native PPX → checked C11 → run; malformed syntax rejected |
| **M2 — Typed metadata and evidence** | Versioned declaration/witness interface, checked typed feedback, codegen/evidence admission | Wrong typed evidence rejected; stale manifests/forgeries rejected |
| **M3 — Constrained generics + coherence** | `forall`-compatible constraints, canonical solver, orphan/overlap/termination, named `using` | Cross-module coherence and multiple specializations tested |
| **M4 — Associated types and multi-parameter protocols** | Kinds/projections and normalization over actual supported On1x types | Equalities checked, ambiguity/cycles rejected |
| **M5 — Module signatures and functors** | Native `Functors` PPX, canonical module identity, sealing, applicativity, explicit generativity | Correct abstract identity, separate compilation and privacy tests |
| **M6 — Derivation, laws, library integration** | On1x `Derive`/`Laws` processors and initial standard protocols | Generators typecheck; reproducible property tests; statuses accurate |
| **M7 — Ownership/effect-aware auto protocols** | Requests for `Send`/`Sync`/resource capabilities with independent trusted validation | Negative foreign-resource/aliasing tests; no unsafe auto-admission |
| **M8 — Optional advanced abstraction** | Bounded higher kinds, dynamic protocol objects, controlled specialization | Sound ABI/kind/resource tests and backend parity; no speculative claims |

Do not skip directly to adding dozens of protocol names or SML functor catalogs. Complete M0/M1 **with runnable programs** first. The agent may split milestones into reviewable PRs; no milestone is complete if only AST/schema files exist.

## 20. Agent execution protocol and definition of done

1. Record the repository SHA, existing PPX behavior, current test status, installation layout and inspected compiler constraints.
2. Implement in the dependency order M0 → M8, **always compiling the Category processors from On1x source**. Maintain an accurate feature status table (`unimplemented`, `partial`, `tested`, `verified`) and keep `on1x/docs/roadmap.md` in sync.
3. Create/extend the named `stdlib/ppx` authoring libraries and add tests exercising them independently from each processor.
4. Keep changes to the trusted C++/C11 compiler host **minimal, generic and separately justified**. Any new metadata interface must be versioned, typed, provenance-bound and independently validated. Explicitly identify any unavoidable non-PPX semantic kernel.
5. Build the processor bootstrap without Category enabled, then compile programs with `@enable Protocols` and `--enable Protocols` and demonstrate correct execution.
6. Run the full preexisting scanner/PPX/macro/staging/type/inference/MIR and CMake/CTest suites where available. Fix regressions rather than exempting existing tests.
7. Use strict GCC and Clang where possible, sanitizers, negative tests, import-order metamorphic cases, failure injection, budget limits, cache invalidation and installation/relocation tests.
8. Document syntax, type/effect semantics, instance resolution, functor identity, capability boundary, known limitations and exact build commands in `on1x/docs/category.md`, `on1x/stdppx/category/README.md`, and appropriate existing manuals.
9. Emit a short implementation report showing changed files, processor source paths, verified executable examples, complete test results, missing work and exact unsupported semantics. No fabricated success or guessed coverage percentages.

### Definition of done

The Category system is complete only when `@enable Protocols` (or `--enable Protocols`) can run a **native, On1x-authored processor** that parses real protocol/witness source, elaborates it to **typechecked** On1x, passes deterministic coherence/evidence admission, produces correct C11, and executes passing positive and negative tests. Functors are complete only when real module applications preserve abstract type identity and sealing across imports; generic protocols are complete only when type-directed evidence is validated **after inference**; advanced auto protocols are complete only when ownership/concurrency properties are independently established.

**In short: an opt-in extension whose actual implementation lives in `on1x/stdppx/native/*.on1x` and `on1x/stdlib/ppx/**`, not a forked language compiler or unchecked code-generation trick.**
