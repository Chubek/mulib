# Category: native declaration PPXs, explicit scalar profile v1

This is an executable **M0/M1 slice** of `../CATEGORY.md`, not the complete
Category system. Protocol syntax, substitution, local coherence and lowering
are implemented in `stdlib/ppx/category/{syntax,witness}.on1x` and compiled into
`stdppx/native/protocols.on1x`. No Python/C++ protocol parser or solver is used.

## Use

```on1x
@enable Protocols
@extension Protocols {
  protocol Eq<Self> { fn eq(a: Self, b: Self) -> bool; }
  impl Eq for i64 {
    fn eq(a: i64, b: i64) -> bool { return a == b; }
  }
  witness Different: Eq<i64> {
    fn eq(a: i64, b: i64) -> bool { return a != b; }
  }
}
fn main() {
  println(Eq_i64_eq(42, 42));
  println(Different_eq(42, 42));
}
```

Output: `true`, then `false`. Selectors are **ordinary monomorphic functions**,
not first-class nominal witness values, implicit search or runtime dictionaries.
Canonical selectors use `P_T_member`; named selectors use `W_member`.

```sh
cmake -S on1x -B build/on1x -DBUILD_TESTING=ON -DON1X_BUILD_NATIVE_PPX=ON
cmake --build build/on1x --parallel 4
build/on1x/on1xc --list-extensions
build/on1x/on1xc --explain-extension Protocols
python3 on1x/tools/on1x-build.py on1x/stdppx/examples/category/eq.on1x \
  --on1xc build/on1x/on1xc -o build/category-eq
build/category-eq
```

Remove the directive and pass `--enable Protocols` for CLI activation. The build
helper forwards `--enable`/`--disable`. Repeat/comma-separated CLI selections and
exact source repeats deduplicate. Names are case-sensitive; aliases are not
registered. `--disable Protocols` overrides source directives, including imports;
conflicting CLI selections reject. Other Category features report unavailable.

The bootstrap surface is the generic balanced `@extension Feature { ... }`
gateway. Bare contextual `protocol`/`impl` sugar is not implemented. No core
keywords were added: `fn protocol(impl: i64)` remains valid.

## Semantics and checking

- One module-local nominal protocol parameter named `Self`. Method types are
  `Self`, numeric scalars or `bool`; `void` is allowed only as a result.
- Concrete `impl P for T` and named `witness W: P<T>` require a protocol defined
  in the same module and a numeric/bool target. Every method must occur once with
  the exact substituted signature. Missing/extra methods, duplicate protocols
  and overlapping canonical heads reject independent of declaration order.
  Named alternatives do not enter canonical-head selection.
- Identities are `canonical-origin::protocol::name`, with target/witness name
  appended for witnesses. These are reporting/naming identities, **not** nominal
  runtime types, package authority, interface caches or proof tokens. Same-spelled
  protocols in different modules are distinct. Cross-module evidence is closed.
- Public selector collisions reject. Helpers use bounded fresh names separated
  by SHA256 of the canonical module origin, preventing serial collisions under
  legacy quoted flat imports. They are implementation functions, not sealing.
- Generated annotations come from the **substituted protocol signature**, not a
  PPX `well_typed` flag. Normal compiler checks validate every body, argument,
  result, name, ownership rule and return path. Optional Nerotypical/Chisel checks
  see the same HIR. No privileged HIR, proof constructor, pointer encoding or ABI
  is injected. A user protocol named `Send`/`Sync`/`Copy` grants no capability.
  A generic host-only provenance flag additionally locks generated function
  signatures across later ordinary v1 PPXs: changing, erasing or duplicating
  these interfaces rejects with `ON1X-EXT-INTERFACE`. Bodies still undergo normal
  checking; this is not a law/equivalence proof or a typed generic feedback loop.
- Bodies accept a bounded scalar subset: literals, identifiers/calls,
  parentheses, unary/binary operations, `let`/`var`, `return`, `if`/`else` and
  expression statements. Normal checked arithmetic and left-to-right evaluation
  apply. Body macros, quotes, imports, loops, assignment, lambdas, qualified names,
  aggregates and resource types are unsupported and diagnose.

Selectors can be ordinary function values; that does not make their consumers
protocol-constrained generics. A well-typed equality method is not proof of laws.

## Modules, phases and generic host boundary

Source activation is module-local and precedes the first carrier. CLI activation
applies compilation-unit-wide. Imported directives never authorize importer
syntax; all visited files are checked. The loader retains canonical origins,
aliases, cycle checks and exports. Generated scalar selectors are ordinary
exports; protocol metadata is not imported as compiled/cross-module evidence.

The initial **declaration-only phase split** is deliberate: generic span capture
→ native declaration expansion **before loader export binding** → existing loader
resolution/hygienic macros/ordinary PPX/staging/inference/checking pipeline. This
lets imports resolve generated selectors without rewriting the ordinary AST or
putting Category semantics in the loader. Macros inside carriers reject; ordinary
macros outside them can call selectors. Typed feedback is required before future
generic/associated-type elaboration.

C++ hooks only handle activation, registry resolution, balanced spans, source
identity, bounded transport, schemas, reconstruction of ordinary functions and
retention of their explicit interfaces through later PPXs.
`Self` syntax, member comparison, local head checks and lowering are On1x source.

### Additive syntax transport

```text
(ModuleSyntax 1 origin existing-function-names carriers module-key)
(DeclarationCarrier owner exact-source start-location end-location tokens)
(SourceToken exact-spelling location)
(ModuleExpansion 1 origin (Unit 1 generated-functions) reporting-facts)
(ExtensionFact owner kind name identity location)
```

`ExtensionInput`/`ExtensionOutput` are separate schema categories. `Unit` v1 and
prior field meanings are unchanged; carriers are never forged functions.
`ppx_serve_as` validates these roots; `ppx_serve` keeps its original `Unit` → `Unit`
behavior and protocol-v1 environment. Facts are reporting data, never typing,
ownership or safety certificates, and are not reused as compiler proofs.

Tokens retain exact bytes, offsets, byte-based line/column and module origin.
The On1x reader checks spellings against source slices, nonoverlap, coordinates
and end spans. Bounds: 32 carriers/module, 16,384 tokens/carrier, 256 KiB/carrier,
256 delimiter levels; native limits add 64 declarations/module, 16 methods and
16 parameters/member, 256 generated functions, 64 expression levels/chain nodes,
32 block levels and 256 statements/block. Existing 4 MiB wire, 262,144-node and
subprocess timeout limits apply. Exhaustion diagnoses, not success or an
unsatisfiable predicate. Errors publish no final C or partial interface cache.

### Registry/security

`stdppx/category/features.sexpr` is checked in, versioned and embedded. Names are
not executable paths. Automatic Linux native resolution uses `/proc/self/exe`,
not argv[0], PATH, CWD, `ON1X_STDLIB` or `ON1X_PPX_SCHEMA`. Build artifacts live
in `stdppx/` beside the compiler; installed binaries/schema/pins live under
`libexec/on1x/`. CMake pins version and binary/schema SHA256; the host checks both
before/after invocation. No shell/Python fallback exists. Relocation works without
the checkout or a Python interpreter at PPX execution time.

The installation is trusted like the compiler: hashes are not signatures and do
not defend against replacement of the entire package. Existing MuLIB SHA256 is
compiled once in the compiler-only `src/extensions_hash.c`; applications/native
PPXs keep the ordinary On1x runtime. `@enable` never grants
`--allow-source-ppx`. `--deps` runs no PPX; `--emit-ast` rejects unexpanded carriers.
Dependency inspection has no generated-export interface yet: selective/qualified
references that require those exports may reject until native expansion. It must
not execute a processor merely to answer an inspection request.
Other hosts need an audited executable-location API. Cross builds must disable
native PPXs, not run target binaries on the host. `ON1X_BUILD_NATIVE_PPX=OFF`
disables activation even if stale binaries exist. No Category cache or compiled
evidence interface exists yet.

## Authoring APIs implemented

| Library | Actual APIs |
|---|---|
| `ppx/ppx.on1x` | Existing builders/fresh names; additive `ppx_serve_as` |
| `ppx/tokens.on1x` | `ppx_read_tokens`, `ppx_peek`, `ppx_expect`, `ppx_source_location`, coordinate/error helpers |
| `ppx/source.on1x` | `ppx_source_prefix`, `ppx_fresh_prefixed` |
| `ppx/diagnostic.on1x` | `ppx_diagnostic`, original coordinates/category |
| `ppx/category/syntax.on1x` | Protocol/witness/member/type and scalar expression/block parsers |
| `ppx/category/witness.on1x` | Declaration collection, substitution, local coherence and selector/fact generation |

No solver, typed-query, functor, derive, law or auto-protocol stubs were added.
Shared authoring API tests compile/run independently of the native processor.

## Status

| Stage | Status | Boundary |
|---|---|---|
| M0 activation | partial | Executable Linux/native profile; other-host/cross-host execution closed |
| M1 scalar protocols | partial | Local declarations/selectors; no nominal witness values or generic lookup |
| M2 typed metadata | unimplemented | Facts are not certificates/module interfaces |
| M3 generics/coherence | unimplemented | No qualified schemes, blanket instances or cross-module evidence |
| M4 associated types | unimplemented | No projections/kind solver |
| M5 signatures/functors | unimplemented | No module application, abstract identity or sealing |
| M6 derive/laws | unimplemented | No derived witnesses, law tests or proofs |
| M7 auto protocols | unimplemented | No ownership/thread-safety admission |
| M8 advanced | unimplemented | No higher kinds, dynamic witness ABI or specialization |

Examples: `stdppx/examples/category/{eq,ordering}.on1x`. Tests:
`tests/test_category.py`. Exact evidence: `../VERIFICATION.md` and
`capability-matrix.md`.
