# On1x Module Semantics

Status: proposed normative implementation contract for the four-worker module-algebra project, not a claim that the features below already exist. The agent must reconcile this contract with the current checkout, record deviations explicitly, and ship executable evidence.

## 1. Baseline and scope

MuLIB inspected at [791ea779453c2c64d5d9dc9fac73c0f446898df5](https://github.com/Chubek/mulib/commit/791ea779453c2c64d5d9dc9fac73c0f446898df5), 2026-11-27, master. LibStone inspected at [4607ce58ea70482a9c057c64557260329d36afde](https://github.com/Chubek/libstone/commit/4607ce58ea70482a9c057c64557260329d36afde), with [firestone/libdsl/LibDSL.hpp](https://github.com/Chubek/libstone/blob/4607ce58ea70482a9c057c64557260329d36afde/firestone/libdsl/LibDSL.hpp), blob b7755c116e1626c2c64ab85fb8cd39737348bd2b. Latest MuLIB commit removes on1x/stdlib; prior cleanup also removed docs/examples. CMake still refers to removed files, including diagnostics, formatter and prettyprint sources. Do not silently restore the deleted library or claim the clean checkout builds. Audit actual intended external dependencies, parameterize roots or isolate the missing components behind truthful configuration gates, and use small self-contained module fixtures. Report any full-suite blocker. Historical references are evidence, not current inputs.

Existing implementation anchors: [loader.hpp](https://github.com/Chubek/mulib/blob/791ea779453c2c64d5d9dc9fac73c0f446898df5/on1x/include/on1x/loader.hpp), [loader.cpp](https://github.com/Chubek/mulib/blob/791ea779453c2c64d5d9dc9fac73c0f446898df5/on1x/src/loader.cpp), module_visibility.hpp, module_signature.hpp/.cpp, module_opaque.hpp, parser.cpp, infer.cpp, sema.cpp, canonical/verify_values.cpp, cli.cpp, ppx.cpp, ppx_ast.cpp, ppx_schema.cpp, grammar/PPX.asdl, grammar/PPX.schema.json, grammar/source_graph.lua, tools/on1x-project.py, SDK frontend/dependency/workspace tests, addons/lsp. Existing tests include test_modules.py, test_module_acceptance.py, test_module_signature_cli.py, test_module_signature.cpp, test_module_visibility.cpp, test_project_module_membership.py, test_ppx_module_metadata.cpp and test_opaque_module.*. Read and preserve these contracts before modifying them.

The old system already has physical source_edges, dependency_sources, signature_dependencies, --deps, --project-module-graph (on1x.project-module-graph/v2), package fragment grouping, interface checking and a bounded direct-C11 executable opaque profile. Unit-v1 PPX intentionally rejects module-scoped import graphs. The new implementation must unify rather than duplicate these mechanisms. Named modules do not automatically re-export imports; legacy package behavior remains compatible. Neither a source graph, a declaration-only conformance result nor serialized metadata authorizes execution.

## 2. Objects, identities and environments

Let P be a verified package identity (canonical origin plus declared package identity, locked revision where applicable); M = (P, qualified-name) a named logical module; F an admitted physical source identity; D = (owner, namespace, declaration-key) a declaration; S a signature; T an On1x type term; and E a finite binding environment. Use tagged identities for legacy anonymous sources, program imports, package initializers, built-ins and functor applications; none is silently equated with a named module. Use source-content digests to identify revisions, not to identify equal types from unrelated providers. Internal path-based provenance remains authoritative; portable serialization uses a canonical package-relative or explicitly scoped identity, never an alias as authority. Stable IDs are revision-local unless a separately documented persistence rule applies.

Namespaces distinguish values, nominal types, macros/syntax, modules, clichés and operators (including position/arity/fixity where relevant). A binding carries D, its owner F/M/P, spelling, kind, normalized type or signature, visibility, source span, optional seal and capabilities, not merely a flattened string. An alias is a view onto D, never a new D. Two equal spellings are not equal declarations. Preserve the existing lexical-shadowing rules and hygienic macro definition-site provenance.

A module descriptor is (identity, ordered fragments, associated interface fragments, declarations, imports, export environment, obligations, revision, capabilities). Fragment assembly is ordered by the authorized manifest; physical private declarations retain their owner. A module is static; no implicit initializer execution, dynamic loading, first-class runtime module value or arbitrary capability is introduced. Manifest package admission and source-language visibility are separate predicates.

## 3. Two graphs, one immutable source snapshot

The import graph I is a directed, attributed multigraph over physical sources, with explicit request nodes or uniquely identified request edges. Orientation is requester -> provider. Preserve source order, exact spelling, kind (named, relative, quoted legacy, program, selective, prelude, implicit, re-export, functor-related), aliases, selectors and operator metadata, source byte range, resolution outcome, selected declaration IDs and provider file/group IDs. A multi-fragment provider is represented by explicit membership/target relationships, not by guessing a representative file. Preserve distinct requests even if they resolve to the same target. Unresolved or denied requests may appear only in an explicitly observational partial graph, with diagnostics and no admitted provider claim.

The module dependency graph G is the quotient of admitted computational import edges by logical module ownership, with provenance back to I. Include legacy/program nodes with tagged identities and package boundary information. A physical edge internal to an authorized fragment group is retained in I; quotient-internal edges are recorded separately rather than turned into false inter-module cycles. Genuine recursive self-imports are still diagnosed according to source policy. Signature, membership, dependency-lock and prelude inputs are separately typed relations; do not treat interface files as executable source nodes. Keep an auxiliary dependency relation A and explicit graph roots. Existing project-module-graph/v2 physical projections are compatibility views, not independent authority.

Define q:F -> M_or_legacy. Every admitted inter-module edge in G must have at least one underlying witness in I, and every admitted computational I edge must be represented by an inter-module edge or a recorded internal relation. Store all witnesses, not only the first. Distinct package identities remain distinct even if their display names and module names coincide. Build reverse edges, dependency-first topological order (providers before requesters), reachability, SCCs, transitive closure on demand and deterministic cycle witnesses. Strict compilation rejects forbidden cycles; partial IDE inspection can retain SCCs but cannot admit them. No unrestricted recursive module semantics in this milestone.

Graphs are immutable and transaction-scoped, derived from the exact parsed bytes in the loader snapshot, including overlays. A change to sources, manifest membership, interfaces, policy, dependency lock, relevant compiler options, OML rule digest or trusted root invalidates affected facts and dependents. Do not confuse import reachability with access authority or runtime call reachability. Missing files, ambiguous paths and denied access are explicit states, never dummy providers.

## 4. Module Algebra

The algebra is typed and partial. Results are Ok(value, witnesses) or Err(located diagnostics); partial observations are separate. Let Gamma contain authenticated graph, declarations, requester and package policy. Use judgments Gamma |- request => provider; Gamma |- requester can-access D; Gamma |- M : S => evidence; Gamma |- term => normal-module-result. No failed premise may become a successful empty set.

| Operation | Domain -> result | Intended semantics |
| --- | --- | --- |
| empty | () -> Env | Empty binding map |
| declare | Env x Declaration -> Env | Add authenticated declaration with duplicate and namespace checks |
| compose | ordered fragments -> Module | Authorized same-module assembly |
| resolve | Gamma x Request -> Provider | Canonical resolver result; handles ambiguity and provenance |
| qualify | Provider x alias -> Env | Namespace binding, not authority change |
| select | Provider x selectors x requester -> Env | Accessible exports only; exact selector kind and spelling |
| rename | Env x injective mapping -> Env | Change exposed names without changing owner, seal or identity |
| merge | Env x Env -> Env | Compatible union; reject distinct declarations colliding in same namespace |
| hide/project | Env x names -> Env | Restrict view; no privilege widening |
| access | requester x Declaration x policy -> Decision | File/package/hierarchy/dependency checks |
| reexport | Env x export policy -> Env | Explicit projection of eligible bindings, original authority retained |
| instantiate | Signature x type arguments -> Signature | Capture-avoiding substitution and arity checks |
| include | Signature x Signature -> Signature | Compatible signature composition under duplicate policy |
| share | Signature x type equation -> Signature | Kinded type-equality constraint, normalized with occurs check |
| conform | Module x Signature -> Evidence | All specified members/types match, against original provider |
| seal | Module x Signature x mode -> ModuleView | Restrict exports and assign opaque or transparent identities |
| apply | Functor x ModuleArgs x site -> Module | Checked static functor instantiation |
| depend | Graph x Module -> dependency closure | Graph query, not admission grant |

Define access for authenticated declaration owner O and requester R: same canonical file permits access; otherwise private rejects. Shared(k) requires equal proven package identity and prefix(R.name, prefix(O.name, depth(O.name)-k+1)), with 1 <= k <= depth(O.name); maximum elevation retains the first component, not the whole package. Public and unannotated require existing same-package or dependency admission. Unspecified annotation is retained during interface merge; conflicting explicit annotations reject. These are access gates, not substitutes for export projection.

Composition is not arbitrary commutative union: manifest order and declaration collisions matter. Compatible environment merge has empty as identity and is associative where both parenthesizations are admitted and preserve identical provenance; idempotence applies only to the same authenticated binding, not to two definitions with matching text. Selection and hiding are monotone restrictions: project(project(E,A),B) = project(E,A intersect B); projection never adds authority. Injective renaming composes and preserves identity, type and seal. For authorized aliases, repeated resolution returns the same provider. Re-export access is the intersection of the forwarding policy and original owner policy, not their union. Conformance and sealing do not create executable admission by themselves. Check all laws with generated bounded environments and negative counterexamples; do not assert unrestricted algebraic laws for effectful/ordered operations.

## 5. Signatures and executable abstraction

Preserve legacy .on1xi obligations versus new .on1xi export sealing. Keep abstract module A; and module A : S; syntax, named signature definitions, type parameters, includes and sharing equations. Normalize equations with capture-avoiding substitution, kind/arity checking, deterministic representatives, occurs checks and bounded expansion. Conformance is against actual original provider declarations, not re-exported impostors or flattened aliases. Signature members are checked for types, procedure/function distinction, ownership/borrowing and visibility; missing, duplicate, inconsistent or capability-incompatible members reject.

Opaque identity is provider identity + member + arity + canonical type arguments, plus the appropriate seal/application identity. Transparent views preserve the provider's nominal identity and permitted manifest equality. A client cannot project, construct, pattern-match, reinterpret, coerce or infer hidden representation from shape, including through generics, closures, PPX, FFI or synthesized aliases. Provider representation access uses host-authenticated ownership. For existing concrete direct-C11 opaque support, retain the established restrictions during migration.

Expand executable admission in tested increments: transparent and manifest type views; parameterized opaque types; nested aggregates and type equations; managed/unique and recursive ownership. Each increment needs canonical representation checks, layout and calling convention, move/borrow/drop behavior, failure cleanup, cross-provider mismatch negatives, and explicit backend capability gates. C11 can use provider-owned handles for hidden/managed representation or verified ABI-safe concrete transport; choose and document representation per profile. No shape-based casts or unsafe implicit widening. No promise of universal backend parity: unsupported MIR/VM/native routes must reject before artifact publication until their verifier and lowering implement the needed semantics.

## 6. Functors and separate compilation (new work)

Implement a bounded static first-order module functor facility with typed module parameters, result signature, application and explicit identity mode. Proposed surface syntax (to settle against the parser): functor Make(X : INPUT) : OUTPUT { ... }; module Client = Make(Provider);. Use a reserved or contextual syntax that does not change legacy parse meaning. Functors are not runtime closures. Check actual provider signatures, substitute module/type references capture-free, instantiate declarations hygienically and check result conformance before publishing. Preserve actual argument identities in sharing equations.

Default proposed semantics: generative applications distinguish distinct authenticated application sites even with equal arguments; applicative mode, if implemented, is explicit and keyed by normalized functor identity and ordered argument identities. Repeated resolution of the same application site in one snapshot is idempotent. Define revision/cache identities separately from nominal identity. Reject missing/extra arguments, recursive instantiation, cyclic functor graphs, unavailable capabilities and expansion-budget exhaustion. Do not confuse parameterized signatures with executable module functors.

Implement an explicit opt-in separate-compilation profile, starting with closed, concrete C11-exportable signatures. Compile provider once into an object/archive plus versioned checked interface/ABI manifest; compile consumer against that contract; link using deterministic symbol identities and dependency pins. Fingerprint target triple, compiler/OML schema, data layout, calling convention, runtime ABI, relevant options, signature and dependency revisions. Link rejects stale/incompatible manifests before publishing executable. Prove provider object reuse and consumer build without provider body; ordinary combined Unit remains compatibility path. Generics/functors require a defined instantiation strategy; unsupported profiles must be located errors. Private representations stay out of public interface headers except deliberately transparent contracts; managed resources crossing a boundary need provider-defined transfer/destruction contracts and real cleanup tests. Do not claim separate compilation from merely splitting one emitted C file.

## 7. On1x Module Language (OML)

OML is a compiler-internal semantic DSL implemented in on1x/src; authored .oml files live exclusively under on1x/src/logistics. Suggested host files src/module_algebra.hpp/.cpp and src/module_graph.hpp/.cpp (public API headers under include/on1x only when needed), with a narrow source-fact adapter and immutable result adapter. Suggested logistics files: module-language.oml, imports.oml, visibility.oml, signatures.oml, composition.oml, functors.oml, dependencies.oml. Split or combine only if documented. This is not a second user-level On1x syntax or a general-purpose plugin scripting system.

Use the actual Firestone LibDSL C++20 LanguageSynthesizer: dsl::DSL<Host,dsl::LanguageSynthesizer>, dsl::synth::Specification::from_text or SpecBuilder, Host::synthesize, MetaLanguage, MetaEvaluator and AttributeEngine<State,Value>. The upstream [specification test](https://github.com/Chubek/libstone/blob/4607ce58ea70482a9c057c64557260329d36afde/firestone/tests/libdsl/specification.cpp) demonstrates language forms, macros, metaprocedures and native state; [attributes test](https://github.com/Chubek/libstone/blob/4607ce58ea70482a9c057c64557260329d36afde/firestone/tests/libdsl/attributes.cpp) demonstrates dependency declarations, inherited/contextual attributes, bounded evaluation and invalidation; [execution test](https://github.com/Chubek/libstone/blob/4607ce58ea70482a9c057c64557260329d36afde/firestone/tests/libdsl/execution.cpp) demonstrates executable semantics and contracts. Verify actual APIs against pinned source rather than inventing method names.

OML authored specifications must drive real module semantics. Native primitives provide narrowly scoped facts, typed operations and host verification; rules orchestrate them and express policy, composition and judgments. Do not implement the complete algebra in C++ and let .oml merely name or call a monolithic legacy operation. The host can independently validate invariants and run migration differential tests. Use no runtime filesystem probing, Lua hooks, shell/network I/O, arbitrary native callbacks, or untrusted rule loading from source or PPX. Policy rules are trusted build/install assets with pinned digests.

Illustrative LibDSL specification skeleton, not a complete or pre-tested OML program:

```lisp
(language On1xModuleLanguage
  (form resolve 2)
  (form select 3)
  (form merge 2)
  (form seal 3)
  (form depend 2)
  (metaproc import-view (lambda (requester request)
    (resolve requester request)))
  (diagnostic MissingProvider "module provider is unavailable"))
```

Choose actual registered native primitives and executable/attribute bindings that give these forms meaning. Define input/output type schemas for all operations; build a real synthesis and execution smoke test before migrating admission. The .oml assets must be loaded/embedded through deterministic CMake dependencies using ON1X_LIBSTONE_ROOT/firestone/libdsl/LibDSL.hpp or a pinned compatible package, with no silent fallback to ad hoc semantics. An intentionally disabled feature must advertise its absence and reject affected modes; a release with OML cannot silently run a parallel legacy authority. The compiler must not require LibDSL in emitted user executables.

Use attributes for request resolution, provider exports, requester access, normalized signatures, seal identities, dependency closure and cache invalidation; express declared dependency relationships explicitly. Attribute keys use stable host IDs, never pointers. Use bounded dependency evaluation and deterministic topological/SCC algorithms. Circular attributes may use an explicitly bounded monotone fixed point only for finite domains with documented convergence; never use fixed points to make forbidden import cycles legal. Preserve structured diagnostics with rule and source provenance. Every stage runs on a private immutable snapshot or transaction, commits only after verification, and has predictable resource budgets.

## 8. Serialization and tooling

Introduce additive driver modes, proposed names --emit-import-graph and --emit-module-graph; --emit-module-algebra for optional canonical OML facts/results; --graph-format json|sexpr|dot; --graph-partial for explicitly observational IDE output. Settle naming before publishing, update help/formatters as appropriate, keep --deps and --project-module-graph/v2 semantics byte-compatible where tests require. Reject conflicting modes. Graph extraction must not execute source PPX, extensions, foreign code or user programs. Respect existing --project-module-index, --stdlib, --no-prelude, search paths and workspace overlays. A full graph can distinguish resolved versus checked, and must not imply type/body verification that never ran.

Proposed versioned canonical JSON envelope:

```json
{
  "schema": "on1x.import-graph/v1",
  "status": "resolved",
  "snapshot": {"id": "sha256:...", "policy": "sha256:...", "oml": "sha256:..."},
  "roots": ["f:entry"],
  "packages": [],
  "nodes": [],
  "requests": [],
  "edges": [],
  "auxiliary": [],
  "diagnostics": []
}
```

Use a parallel on1x.module-dependency-graph/v1 envelope with logical nodes, membership, admitted edges, witness request IDs, internal edges, SCCs and dependency-first order. IDs shown are symbolic examples, not real digests. Define exact field types, enum values, optionality, bounds, escaping, deterministic ordering, revision identity and span semantics in a machine-readable schema and fixtures. JSON is the primary lossless interchange; S-expression is equally lossless, DOT intentionally visualization-only. Serialize 64-bit offsets/IDs safely for JS (strings or documented bounded integers). Handle path privacy: default to package-relative display paths where possible, allow explicit absolute mode, preserve graph identity independent of presentation. Use exact UTF-8 bytes and document byte vs LSP UTF-16 conversions. Reject unknown versions, duplicate IDs, dangling references, contradictory admission, invalid spans, oversized or cyclic inappropriate data. Deserialization yields untrusted observations, never authoritative checked modules.

Make PPX graph transport a separately versioned, host-owned read-only input packet (e.g. SourceModuleGraphMetadata v1) containing the same canonical graph facts, not an unrestricted Unit field. Add explicit negotiation and schema bounds. Preserve legacy Unit-v1 byte shape and its current fail-closed restriction until a safe module-aware protocol is implemented. For that protocol, operate on a single authenticated module/fragment or explicit authorized target, freeze provider identities, import set, scope, seals and capabilities; validate transform output, re-resolve references in the original environment, recheck signatures/ownership/hygiene, and reject forged graph or module metadata. No new import/dependency/capability by returned PPX text. A future graph-changing transform would require a separately authorized host operation and full fresh admission; not implicit here. Ensure external and native PPX see the same allowed metadata with no inherited filesystem grants. No graph inspection may launch a PPX process.

SDK and LSP should consume immutable canonical graph snapshots, not parse source with a second resolver. Add versioned SDK accessors with counts, lifetimes, ownership and compatibility; overlay revisions invalidate dependents. Provide go-to-definition (via declaration identities), import diagnostics, dependency listing, reverse dependencies and cycle explanations. Missing/denied/partial data is visibly labelled; do not make a whole-project build prerequisite for IDE inspection. Test stale snapshots, cancellation/budgets, edited imports, symlinks, paths with spaces/Unicode and identical module names from distinct packages.

## 9. Coordination and acceptance

Exactly four workers: W1 graph, canonical identities, serializer core and loader snapshot adapter; W2 module semantics and language gaps, signature/functor/opaque and separate-compilation implementation; W3 OML/LibDSL synthesis, algebra operations, rule assets and CMake integration; W4 CLI/PPX/SDK/LSP/project tooling, protocol compatibility and integration verification. Assign one owner per shared file; W1 owns loader.cpp integration, W2 owns parser/type/sema/signature edits, W3 owns src/logistics and algebra host plus central CMake edits, W4 owns cli.cpp, PPX and tooling. Cross-owner changes via small reviewed patches or agreed interface handoff, not concurrent editing. W2 may delegate backend-specific work through agreements but remains accountable for executable semantics. W4 leads integrated acceptance; all workers write tests for their own components.

Use a run-specific private directory under /dev/shm (mode 0700, umask 077), with an append-only or atomically replaced locked blotter, per-worker status and a contract directory. Each entry has timestamp, worker, base SHA, phase, owned files, interface version, dependencies, patch/commit, tests and result, blocker and next handoff. Use flock for shared writes and atomic rename for complete snapshots; no credentials, no untrusted symlinks, no polling busy loops, no destructive cleanup outside that run. Use separate worktrees/build directories and named branches, coordinate merges; the blotter is coordination only, not a compiler trust source. Archive durable evidence into repo artifacts before removing the transient run directory.

Gates: (G0) exact revisions, build blockers, baseline fixtures and contract handshake; (G1) graph snapshot, identity and OML synthesis cross-worker smoke; (G2) migrated algebra parity for existing module semantics; (G3) executable signature/opaque/functor slices plus separate-compilation evidence; (G4) driver/PPX/SDK/tooling integration and adversarial tests; (G5) merged clean-tree regression, installation/relocation, capability matrix and reproducible qualification. Do not claim G3 complete if only declaration-checking passes. Fail closed on missing upstream components and retain prior successful artifacts on error.

Minimum test evidence: pure algebra laws with independent reference model and seeded randomized small graphs; differential old/new resolver and graph quotient; exact JSON/sexpr round-trip and deterministic repeated builds; malformed graph/PPX schema tests; package identity, relative imports, alias collision, private/shared elevation, diamond, duplicate selectors, cycle, fragment and re-export negatives; signature conformance, type equation and generic opaque cases; executable direct C11 strict C11 and sanitizer runs where available; functor identity and substitution; actual separately linked provider and consumer and stale ABI negatives; explicit unsupported MIR/VM routes; overlay, cache invalidation and LSP smoke; installed/re-located OML asset checks; independent verification of bounded work, invalid states, failure atomicity and security. Use generated fixtures that do not require the removed stdlib unless an explicitly selected external library is present. Never skip failing existing tests without recording why.

Deliver actual code, .oml sources, tests, schemas, updated help and manuals, examples/fixtures that exist, and on1x/artifacts/MODULE_SEMANTICS_IMPLEMENTATION.md with exact revisions, changes, commands, results, exclusions, performance/cost measurements if collected, and a per-feature matrix distinguishing implemented and executed, declaration-only, rejected, and blocked. Record the OML rule digest and dependency versions. The work is complete only to the extent independently demonstrated; no unqualified production-readiness or formal-proof claim. Preserve this requirements document unless amendments are explicitly recorded and reviewed.
