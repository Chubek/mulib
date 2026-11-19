# On1x roadmap and implementation status

The [extension SDK](../sdk/README.md) is an M7 tooling slice: analyzers and
artifact emitters can receive verified MIR from `on1xc`. Linux x86-64
supports isolated shared-library plugins as well as trusted linked providers.
The SDK does not expose checked HIR or add a native backend.

## Current feature matrix

M1/M2 core-syntax additions use the canonical parser, checker, and C11
backend. [Literals and interpolation](core-literals.md) distinguish Unicode
scalars from bytes. [Regular expressions](core-regex.md) compile through
Firestone and run without LibStone. [Enum arm lists](core-enum-connective.md)
share structural matching.
[Checked shared/exclusive borrows](core-references.md) admit synchronous
`borrow T`/`ref T` parameters and lexical `borrow [mut]` aliases for named
places, including `own<Named>` payloads. They remain non-first-class and expose
no foreign pointer ABI.
[Shared `sexpr` owners in nominal fields](owning-adts.md) use existing RC
cleanup. [Finite unique owners](recursive-owning-adts.md) add recursive nominal
layouts through `own<Named>`, explicit transfer, direct C11 cleanup, and
consuming owner-bearing nominal `Result` propagation. General borrows,
checked external handles/resources, field/index loans, borrowed receivers,
suspension-aware loans, and effectful ownership/loan MIR remain unsupported.

The current defer/Result ownership continuation (2026-11-19) adds checked
active-arm transfer for finite `own<Named>` payloads, proves shared-payload
cleanup through cancellation, and keeps JSON/CSP payloads and unique deferred
actions closed. Strict GCC/generated and sanitizer profiles pass **4/4**;
strict Clang/fallback and sanitizer profiles pass **3/3**. See the
[current verification record](../VERIFICATION.md#function-defer-and-owned-result-closeout--2026-11-19).

The earlier final cross-feature hardening validates bounded checked types,
unique-transfer metadata and independently reconstructed generated member
signatures, binders, constraints and private identities. Generated GCC passes
98/98 tests; fallback passes 97/97; the focused Clang ASan/UBSan matrix passes
15/15. See the historical
[verification record](../VERIFICATION.md#final-type-checker-and-cross-feature-hardening--2026-11-19)
for commands and the remaining full-sanitizer limitations.

[Intrinsic core cliches](core-cliche.md) add coherent constrained generics,
generic members and associated values through compile-time static dispatch.
Exact heads override one blanket head; overlap, orphan and nondecreasing
prerequisite checks reject before lowering. This mechanism is independent of
both inherent `impl` lookup and the legacy Category/PPX protocol.

[Intrinsic nominal implementation blocks](core-impl.md) attach generic
associated functions/values and copy-safe by-value receiver members to records
and enums. Canonical nominal identity, same-module ownership and unique member
names make dispatch deterministic; calls lower to ordinary specialized
functions. Borrow/move or managed receivers, interface/PPX transport and
aggregate MIR remain unsupported.

The [MuLIB contract catalogs](../contracts-exotract/README.md) share a
MANIFEST-backed inventory. CDD declarations can be inspected through
[KernelBind](kernel-bind.md#inspect-requests-and-discover-declarations), but have
no semantic sidecars or callable bindings.

[`GrammarEmbed`](../stdppx/grammarembed/README.md) provides embedded
`grammar`/`token`/`rule` constructs through the ordinary native PPX protocol.
Firestone liblr supplies NFA/DFA generation and LR1/LALR1/LL1 analysis; the PPX
emits checked On1x lexer/parser machinery. Its documented byte-oriented grammar
subset includes named references, alternatives, groups, repetition and skip tokens.

M7 [`LuaEmbed`](../stdppx/luaembed/README.md) is a native On1x PPX using canonical
MuLIB Mune through a trusted build-host bridge. It captures stdout for ordinary
runtime emission or validates generated AST expressions/statements/programs,
including finite LuaEmbed chains and explicitly ordered subsequent PPXs.
This adds no raw-Lua core grammar, application interpreter, safe FFI claim or
authority to register more processors.

The M5/M7 project tools add non-S-expression `On1xfile` builds, directory-local
input declarations, MuLIB Mune policies, and an opt-in Linux build sandbox.
The [On1x formatter](formatter.md) uses the existing PrettyPrint layout and
emission library; its LaTeX and troff listings are not compiler backends.
These are tooling increments, not general separate compilation or interface
type checking.

M7 [editor addons](../addons/README.md) cover `.on1x` and `.on1xi` highlighting in
Vim/Neovim, VS Code/VSCodium, Sublime Text and `bat`. The optional stdio
language server uses the existing compiler CLI for parse/import diagnostics on
save and explicit formatting. It does not type-check or provide completion,
navigation or rename.

The M5/M7 [discovery continuation](search-paths.md) integrates the moved
`stdlib/mulib` package and adds bounded library/stdlib, PPX/stdppx and kernel
search lists with CLI/environment inputs and declarative manifests. It does not
add separate compilation, dependency downloading or foreign safety admission.

**Locked Git acquisition.** Explicit `on1x-project.py lock`/`fetch` commands
resolve full commit pins or branch/tag intent to immutable commits and stage
[On1xfile-certified Git libraries](../stdlib/on1xfile/README.md#locking-and-explicit-remote-git-acquisition)
from local object stores or smart HTTPS. Canonical lockfiles, atomic
content-addressed cache generations and frozen/offline builds are implemented
on Linux. There is no registry, version solver, transitive fetching,
authenticated/SSH remote, or complete distributed toolchain.

Latest CSP continuation: [cooperative cancellation and monotonic deadlines](cancellation-deadlines.md)
execute through checked HIR/C11, including `taskgroup within` with terminal
timeout after cleanup (private ABI v4). Typed timeout recovery and async/await
remain absent; async requires verified suspension/frame lowering, not a wrapper
around stackful fibers.

| Area | State | Evidence / limits |
|---|---|---|
| Core C++20 compiler | Implemented subset | CMake build, CTest |
| LibStone ASDL/Aurocks integration | Verified P0 subset | Both generated C sources compiled with GCC/Clang; external checkout remains read-only |
| Fallback lexer | Implemented and tested | Bootstraps without LibStone |
| Scanner parity and token lifetime | Verified P0 subset | Deterministic byte/offset/error corpus, including macro/PPX/staging/dotted syntax; repeated and concurrent scan tests |
| Parser / typed AST | Implemented functional subset | Functions, tuples/patterns, explicit universal types, scopes, control flow, imports |
| Intrinsic core additions G1/G2 | Executable control/matching/value-loop slice | Loop/for-C/i64 ranges, labels/exits, guarded structural matches and typed value-loop joins/initialization verification; C11 plus scalar/tuple MIR; nominal-data and G3 continuations below; no owning lists/iterators or concurrency; [manual](core-syntax.md), [value loops](value-loops.md) |
| G3 function defer / typed errors | Executable bounded C11 profile | Go-style function-scoped `defer call;`, registration-time snapshots, function-owned LIFO records, exact nominal `Result` postfix `?`, shared-`sexpr` retention and finite-unique active-arm transfer, return/cleanup edges including cooperative CSP cancellation; no unique deferred captures, JSON/CSP Result fields, using, exceptions, terminal-trap unwind or MIR/SDK defer admission; [contract](defer-errors.md) |
| Module loader / pervasive environment | Executable compile-time namespaces and generic core facade | Named/selective/relative imports, aliases, packages, canonical deduplication, bounded cycles, demand-loaded prelude, relocatable installation, and canonical bounded nominal `Option`/`Result` helpers with explicit/local shadowing; direct concrete owner-bearing Result propagation is admitted, while generic move-only helpers and Option propagation remain closed; [manual](modules.md) |
| MuLIB native housekeeping | Implemented bounded kernel/fixture planning | Integrated `stdlib/mulib`; `std.mulib` metadata/byte access and size checks; shared one-TU planning, fixture/provider/native companion linkage, actual selected header paths/fingerprints and relocatable bundles; [manual](../stdlib/mulib/README.md) |
| PrettyPrint | Executable document DSL, raw C11 bridge and formatter tool | Global Knuth–Plass for `Fill`, semantic math, plain-UTF-8 source maps, block-incremental C sessions and `.ppg` → Python; no safe import, glyph justification or full Oppen; [manual](../stdlib/prettyprint/README.md) |
| On1x source formatter | Executable conservative source layout | Canonical scanner/parser, preserved comments/literals/carriers and dot adjacency, `.on1xfmt` bounded layout search and Allman braces, paired directives, plain/LaTeX/groff, atomic plain-source `--inplace`/`-i` on POSIX; no type checking, syntax recovery or unchanged diagnostic locations; [manual](formatter.md) |
| ChatGPT repository-knowledge addon | Partial hosted integration | Seven bounded read-only MCP tools over a startup-only allowlisted repository snapshot, with cited paths/lines/hashes and tested packaging/relocation. No compilation, execution or safety admission; hosted TLS/auth and actual ChatGPT web interoperability remain operator validation; [manual](../addons/chatgpt/README.md) |
| Search lists / manifests | Executable Linux build-time subset | Five colon-separated CLI/environment path lists; existing named imports, explicitly activated PPX, native bundle/script metadata and MANIFEST-backed kernel lookup; [manual](search-paths.md), `test_discovery.py`; no downloads or new safe-FFI claim |
| Frontend input limits | Implemented and tested | 2 MiB per file; 256-level parsing/HIR limits including lambdas; bounded capture metadata |
| Diagnostics and scrutiny | Executable bounded tooling slice | Build-validated JSON v2 owns new identities/presentation/policy/repairs/PPX declarations, with runtime replacement and trusted bootstrap; bounded registry, text/JSON/SARIF with UTF-16 columns, CLI/source/On1xfile precedence, message-free registered PPX sideband, independently checked Syren edits and explicitly advisory Wetstone proposals; legacy compiler text migration, CFG/interprocedural/resource proof and LSP remain open; [manual](diagnostics.md) |
| Type inference | P1 schemes and explicit universals | Global recursive-component and value-restricted immutable let/pattern generalization, rigid `forall` parameters, bounded C11 specialization/bundles; no defaulting or inferred polymorphic recursion |
| Shared/exclusive place borrowing | Executable bounded HIR→C11 profile | Non-first-class `borrow T`/`ref T`, lexical `borrow [mut]`, nested reborrows, joins, owner-payload access and move/drop invalidation; direct private calls only. No PPX/interface/MIR/FFI/callback/defer/concurrency transport, field/index origins, separate compilation or independent Chisel/Nerotypical admission; [manual](core-references.md) |
| Intrinsic core cliches | Executable bounded C11 profile | Generic declarations/implementations/members, associated values, qualified constrained calls, imports, exact-over-blanket coherence, orphan/overlap checks and bounded recursive resolution; static generated functions only. No associated types, dynamic dictionaries, Unit-v1 PPX transport or separate compiled interfaces; [manual](core-cliche.md) |
| Intrinsic nominal `impl` blocks | Executable bounded C11 profile | Generic targets/members, associated and receiver functions/values, canonical module lookup, enum receivers, deterministic private-function lowering and copy-safe value receivers. Scalar associated members also use existing MIR; `ref self`, `move self`, and managed/resource receivers reject, so owners use explicit non-receiver parameters; no explicit member type arguments, visibility modifiers, Unit-v1 PPX/interfaces or aggregate MIR; [manual](core-impl.md) |
| Structural function types / values | P1 subset | Monomorphic aliases, parameters/results, indirect/postfix calls and lexical resolution; no foreign callbacks |
| Anonymous functions / lexical closures | P1 immutable subset | Escaping scalar/function/tuple captures, transitive capture, reference-counted C11 environments; no mutable/resource captures or recursive let |
| Tuples / destructuring | P1 product subset | Structural products, nested immutable local/function/lambda patterns, leaf schemes and function/universal-field ownership; no JSON elements or aggregate extern ABI |
| C11 AOT code generation | Verified subset | Strict GCC/Clang compile/run tests; ASan/UBSan also cover generated C |
| Core/MIR v1/v2/v3 | B1 scalar/trivial-product subset | Legacy formats preserved; v2 f64/products; v3 unsigned/f32 operations/conversions and numeric tuples with allowlisted descriptors, independent evaluation and C11 emission; bounded verification/round-trip; see [manual](backend-mir.md) |
| MIR reference execution / C11 | Implemented scalar subset | Five-way HIR-C11, plain/optimized MIR-C11 and evaluator tests; malformed CFG/phi/use/effect/initialization rejections; optional CLI and build-helper path |
| Novostone MIR analyses | B3 v1 scalar metadata subset | Opt-in Datalog/CCDB, exact fact checks, transactional publication, revision-bound queries, call/effect closure and block liveness; v2 fails closed pending complete descriptor mapping; [manual](backend-analysis.md) |
| Transactional MIR passes | B4 scalar subset | Opt-in checked constant folding, constant-branch cleanup, local-slot forwarding/overwritten-store removal, dead pure-value/phi elimination; checked preservation, bounded rollback/fallback, deterministic provenance and exact-revision fact publication; [manual](backend-passes.md) |
| Signed i64 checking | Implemented | Overflow/invalid division tests |
| f64 values/literals (M1 slice) | Executable HIR/MIR/C11 subset | Decimal literals, binary64 operators/conversions, bit transport, hex print, raw double ABI and reviewed scalar adapters; Chisel and independent numeric typing; Linux x86_64 GCC/Clang only; no float folding or other-target certification; [contract](numerics.md), [matrix](capability-matrix.md) |
| Unsigned widths / f32 (M1 slice) | Executable HIR/MIR-v3/C11 scalar subset | Distinct u8/u16/u32/u64/f32; suffixed literals, checked arithmetic, explicit wrapping/bit/conversions, numeric tuple MIR/evaluator/emitter, independent Nerotypical type obligations and Chisel admission. No signed narrow/pointer-sized types or verified alternative floating targets; [contract](numerics.md) |
| Native ABI externs | Explicit raw subset | Exact scalar ABI; explicit nested parameter patterns flatten left-to-right, no aggregate layout inference |
| Typed hygienic macros | Recursive patterns and lexical syntax bindings | Context marks, typed recursive captures/ellipses, `let_syntax`/`letrec_syntax`, definition-site value/syntax identities and bounded transactional expansion; not the full Scheme language |
| External AST PPX | Core implemented | Source opt-in / CLI registration, bounded shell-free POSIX transport, S-expression schema validation, labeled carriers and namespaced AST extensions |
| Category M0/M1 | Executable native/local scalar slice | `@enable`/`--enable`, pinned native registry, balanced module carriers, On1x protocol/witness parser and concrete checked selectors; no generics, imported evidence, associated types, functors/sealing or auto protocols; [manual](category.md) |
| Category M2 concrete evidence | Executable bounded slice | Host-retained requests, native post-inference planning, independent member/forwarding admission at check/C11/MIR ingress, deterministic `--emit-category-plan`; processor v2. Persisted/imported interfaces and generic evidence remain absent; [manual](category.md) |
| PPX authoring / standard processors | Native self-hosted libraries/tools | On1x codec/visitor/schema/PPX libraries, fifteen ordinary C11-native processors, the opt-in native Category processor and 24 Python processors; compiler remains C++ |
| Extension standard libraries | Executed raw/persistent subset | Validated map/Option/Result/graph helpers, signed scanner, bounded canonical MuLIB serialization/hash/socket adapters with automatic linkage, status tuples, allocation/loopback tests; no general ownership or f64 ABI proof |
| Multi-stage programming (MSP) | Typed closed-code AOT | `code<T>`, expression and lexical statement-region quote, splice/lift/contextual run, staged returns, quotation-world checks, reference-counted factories/thunks; no JIT, runtime source eval, JSON/unique stage transfer or effect inference |
| Native authoring data/I/O | Bounded C11 substrate | Persistent managed `sexpr`, shared `buffer`, byte operations/I/O; process-lifetime bounded text snapshots, not a general String/ADT/module library |
| JSON resource facade | Experimental, real-kernel tested | Nested views, lexical drops, numeric bounds and injected allocation failures; errors still terminate |
| ExoML contract tooling | Optional public SDK integration | Real libclang extraction, lossless v2 CDecl transport, strict sidecars, pin/check/explain and locked source/toolchain/build-plan evidence |
| JSON/Vec/Parsec contract inventory | 45 unreviewed source annotations | All 20/7/18 core declarations covered; conditional transfers, borrows, errors, effects, callbacks/unknown obligations retained; not safe imports |
| ExoML raw scalar adapter emitter | Optional, narrow tested subset | Signed/unsigned integers, `_Bool`, binary32 `float`, binary64 `double`, void result; exact C11 signature/type/size/alignment/format/range checks and full manifests; explicitly unsafe |
| Reviewed scalar-only safe admission | Executable optional M2 prerequisite | v1 signed-integer compatibility; v2 bool/f64; v3 unsigned/f32. All-or-nothing reviewed ExoML profile, generated facade/evidence, Chisel use-site admission and strict executable target boundary gate; no pointers/resources/errors/callbacks/aggregates; [manual](contracts.md) |
| Reviewed immutable C-string admission | Executable optional M2 slice | v4 direct `const char *`, immutable call borrows/static results, directional use-site evidence and bounded boundary checks; no owned/retained buffers or promoted bridge contracts; [manual](ffi-strings.md) |
| General C FFI adapters | Not implemented | Needs pointer/buffer/aggregate ABI, recoverable errors, ownership/lifetime lowering, callbacks and target matrix |
| Chisel resource checking | Opt-in scalar/JSON usage subset | Real solver plus conservative joins, token-use and exit/reinitialization checks; generic external resource CFG API; [manual](memory-safety.md) |
| Nerotypical structural verification | Opt-in concrete HIR checker | Real TypeGraph/Satie EUF obligations for calls, closures, tuples, staging and resources; explicit universals reject; separate patched source SDK; [manual](type-safety.md) |
| Exotract static bindings | MANIFEST-backed kernel and companion archives | Typed CDecl/import metadata; executable JSON/Vec/Parsec and MicroJS C clients, On1x JSON/YANTLR/Shiraz archive linkage, fixture provenance and one kernel implementation TU; [manual](../contracts-exotract/README.md) |
| Compiler-native KernelBind | Discovery, unsafe scalar calls and review-gated scalar/numerical calls (Packages B–D partial) | ExoLib/CDecl, independent Ed25519 review, one-TU archives, atomic admission bundles, all three Clamp functions, and bounded snapshots and nominal results for Histprice and PriceOpt. No production approvals or general owned resources; Shylock release gate remains open. [Manual](kernel-bind.md), [numerical profile](kernel-numerics.md), [source migration blockers](source-migration.md) |
| Sandbox KernelBind profiles | Build and discovery only | Real ExoLib, CDecl v2 and unreviewed ExoML sidecars for all eight sandbox kernels; one C11 TU. No callable imports, process/capability lowering or enforcement claim. [Manual](kernel-sandbox.md) |
| Wetstone / general foreign ownership lowering | Not implemented | Contract-driven external handle types, ABI, alias/extent and effect proofs remain open |
| Flintstone/Firestone native lowering | Not implemented | C11 is the sole backend |
| Value data model (M1 slice) | Executable by-value C11 subset | Structural records with canonical labels, closed sums with exhaustive `match`, `array<T,N>` and owning `slice` snapshots, bounds-checked access with exit-70 errors; no MIR lowering, mutation, borrowed/dynamic arrays, derives or foreign aggregate ABI; [contract](data-model.md) |
| Nonrecursive nominal records/enums | Executable bounded C11 profile | Invariant parameters (including phantoms), inferred arguments, generic specialization/forall, constructor catalogs, exhaustive structural matching and imported type identity; independently reconstructed layouts; [manual](nominal-adts.md) |
| Recursive nominal ADTs / unique owners | Executable bounded HIR→C11 profile | `own<Named>`, `box`, explicit whole-value moves, consuming take/drop/match/Result `?`, path-sensitive whole/projected moved-state verification, lexical/overwrite/partial-construction/error cleanup, active-arm destruction, iterative traversal/drop and non-owning-defer composition; whole payloads compose with checked lexical/call borrows, but no MIR, PPX, raw FFI, capture/deferred arguments, staging/concurrency or receiver ownership; [contract](recursive-owning-adts.md), [Result contract](defer-errors.md) |
| Module signatures / functors | Not implemented | Interface ownership/effects, generative/applicative identity and separate-compilation ABI remain absent |
| Cooperative fiber / channel G4 slice | Executable bounded C11 profile | `taskgroup`/`within` (terminal timeout), eager `launch`/`join`, consuming cooperative internal `cancel`, 1:1 i64 uni channels with bounded FIFO/rendezvous, checked outcomes and atomic mixed i64/u8 `poll` with monotonic `after`; POSIX `xfiber` direct exec/join/cancel; compiler-parsed backtick argv and affine bounded `u8` streams; MuLIB kernels through one implementation TU; no Plaid integration, full shell operators/expansion, typed timeout recovery, async or native workers; [contract](concurrency.md), [cancellation/deadlines](cancellation-deadlines.md) |
| Plaid/Prolific integration | Not implemented | The cooperative G4 adapter uses MuLIB, not Plaid; profiling and broader concurrency remain open |

## Immediate next iterations

The M2/G3 continuation finishes the already-designed function-scoped defer and
nominal result propagation contract. Dynamic registrations persist across block
and loop exits, retain copied RC operands, and run before remaining lexical
cleanup on explicit/implicit/error returns. The existing value-region verifier
checks tag domination, initialized payload joins, unique active-arm transfers and
exact return types across function boundaries. The finite-owner continuation
moves owner-bearing `Ok`/`Err` payloads, consumes their result source, and
protects a pending managed return while non-owning defers run. Next
error/resource work remains unique deferred captures/actions, JSON/CSP Result
payloads, general foreign error adapters, effectful cleanup MIR and independent
SDK models; this is not general exception or typed cancellation support.
Concrete finite owners transfer through both arms with partial-acquisition
cleanup. Further work includes ownership-constrained generic helpers, checked
foreign handles, and owner-bearing deferred actions.

The language-only M1 continuation adds decimal f64 literals with scanner/PPX
parity and a versioned float/trivial-tuple MIR execution path. A later M2
prerequisite opens only the reviewed scalar safe-import profiles (signed/unsigned
integers, `_Bool`, binary32 `float`, binary64 `double`); it adds no product-specific kernel and
does not admit resources. v1 evidence remains signed-integer-only; v2 adds bool/f64
and v3 adds unsigned/f32, checking parameter and result types at declarations and callsites.

A second M1 slice adds the bounded value data model: structural records, closed
tagged sums with exhaustive `match`, fixed arrays and owning snapshot slices,
all by-value C11 with bounds-checked access and terminal located value errors
([contract](data-model.md)). Nominal/parametric data and finite recursive
`own<Named>` values now have separate direct-C11 profiles. Remaining blockers
include aggregate/ownership MIR, data derives (equality, ordering, printing),
mutable or borrowed arrays, general foreign ownership, cleanup/effect
descriptors, independent checker support and async lowering.
No general numeric/data or concurrency milestone is marked complete.

The backend continuation provides scalar B1, opt-in B3 relational analyses and
transactional B4 scalar transformations. Next backend increments include
restricted Sequent integration and broader MIR aggregate/closure support. Full B1
resource/error-edge support, general B3 contract dependencies, ownership-aware B4
transforms and B5 VM/native lowering remain open.

1. **P0 baseline verified** — Generator-backed and independent fallback builds,
   exact scanner parity, bounded parsing, strict generated C11 and real MuLIB
   JSON integration pass on the tested x86_64 Linux toolchains. Keep these
   regression gates when evolving syntax. See [verification](../VERIFICATION.md)
   for versions, commands and remaining limits; other targets are not certified.
2. **P1 in progress** — Structural monotypes and first-class monomorphic
   functions now have a runnable C11 path, bounded unification/grounding,
    inferred parameter/result holes, immutable lexical closure conversion, tuples
    and local destructuring, with dedicated semantic/syntax/ownership tests.
    Global recursive components and immutable value/pattern lets support schemes;
    explicit universal parameters use rigid checks and typed bundles. Extern
     tuple patterns explicitly flatten scalars. Records and bounded nominal ADTs
     now execute; whole-place shared/exclusive loans and unique-owner payload
     borrows also execute through direct C11. Next ownership work is typed MIR
     and field/receiver/foreign borrow-resource composition. Mutable/resource
     capture lifetimes require further analysis.
    The semantic HIR remains C++-owned, not a new generated ASDL semantic AST.
    The metaprogramming core runs after parsing and before inference, with a real
    ASDL-generated wire tree in integrated builds. Follow-up metaprogramming work
    includes generated transformer definitions, general modules/ADTs, IPC and reviewed processor
    integrations. Recursive patterns, native authoring tools and typed AOT MSP
    now have executable implementations; see [the manual](metaprogramming.md).
3. **P2** — The reviewed scalar-only gate pipeline is executable. Next,
   independently review production [source contract profiles](contracts.md),
   then extend exact target-checked C11 wrappers to pointer-based MuLIB
   JSON/Vec/Parsec contracts; add `Result`/`Option`, borrow regions and explicit
   success/failure transfers.
4. **P3** — Integrate Chisel region/ownership analysis, Wetstone foreign
   function effects and Flintstone high-level dialect. Generate checked
   cleanup on more than return paths.
5. **P4** — Add modules/functors, first-class effects, callbacks, threads,
   coroutines and mixed native/VM backends using Firestone/Plaid.
6. **P5** — Incrementally cover all suitable MuLIB libraries with reviewed
   contract inventories, manifests and reproducible build planning.

## Non-negotiable design boundaries

- ExoML owns canonical C ABI discovery and semantic-contract data.
- MuLIB owns C implementation and documented resource semantics.
- On1x owns syntax, type/effect inference, resource-safe API construction and
  C11/native code generation.
- No claim of safe foreign calls from raw C header discovery alone.
- No hidden runtime ExoML/LibStone dependency for AOT-generated executables.
