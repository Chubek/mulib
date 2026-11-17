# On1x roadmap and implementation status

## Current feature matrix

| Area | State | Evidence / limits |
|---|---|---|
| Core C++20 compiler | Implemented subset | CMake build, CTest |
| LibStone ASDL/Aurocks integration | Verified P0 subset | Both generated C sources compiled with GCC/Clang; external checkout remains read-only |
| Fallback lexer | Implemented and tested | Bootstraps without LibStone |
| Scanner parity and token lifetime | Verified P0 subset | Deterministic byte/offset/error corpus, including macro/PPX/staging/dotted syntax; repeated and concurrent scan tests |
| Parser / typed AST | Implemented functional subset | Functions, tuples/patterns, explicit universal types, scopes, control flow, imports |
| Intrinsic core additions G1/G2 | Executable control/matching/value-loop slice | Loop/for-C/i64 ranges, labels/exits, guarded structural matches and typed value-loop joins/initialization verification; C11 plus scalar/tuple MIR; nominal-data and G3 continuations below; no owning lists/iterators or concurrency; [manual](core-syntax.md), [value loops](value-loops.md) |
| G3 function defer / typed errors | Executable bounded C11 profile | Go-style function-scoped `defer call;`, registration-time snapshots, function-owned LIFO records, exact nominal `Result` postfix `?`, typed return/cleanup edges; no using, exceptions, terminal-trap/cancellation unwind, managed Result fields or MIR/SDK defer admission; [contract](defer-errors.md) |
| Module loader / pervasive environment | Implemented compile-time namespaces | Named/selective/relative imports, aliases, packages, canonical deduplication, bounded cycles, demand-loaded prelude and relocatable installation; [manual](modules.md) |
| MuLIB native housekeeping | Implemented bounded kernel/fixture planning | `std.mulib` metadata/byte access and size checks; shared one-TU planning, fixture/provider/native companion linkage, fingerprints and relocatable bundles; [manual](../std/mulib/README.md) |
| Frontend input limits | Implemented and tested | 2 MiB per file; 256-level parsing/HIR limits including lambdas; bounded capture metadata |
| Type inference | P1 schemes and explicit universals | Global recursive-component and value-restricted immutable let/pattern generalization, rigid `forall` parameters, bounded C11 specialization/bundles; no defaulting or inferred polymorphic recursion |
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
| PPX authoring / standard processors | Native self-hosted libraries/tools | On1x codec/visitor/schema/PPX libraries, fifteen ordinary C11-native processors, the opt-in native Category processor and 24 Python processors; compiler remains C++ |
| Extension standard libraries | Executed raw/persistent subset | Validated map/Option/Result/graph helpers, signed scanner, bounded canonical MuLIB serialization/hash/socket adapters with automatic linkage, status tuples, allocation/loopback tests; no general ownership or f64 ABI proof |
| Multi-stage programming (MSP) | Typed closed-code AOT | `code<T>`, quote/splice/lift/contextual run, quotation-world checks, reference-counted factories/thunks; no JIT or runtime source eval |
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
| Wetstone / general foreign ownership lowering | Not implemented | Contract-driven external handle types, ABI, alias/extent and effect proofs remain open |
| Flintstone/Firestone native lowering | Not implemented | C11 is the sole backend |
| Value data model (M1 slice) | Executable by-value C11 subset | Structural records with canonical labels, closed sums with exhaustive `match`, `array<T,N>` and owning `slice` snapshots, bounds-checked access with exit-70 errors; no MIR lowering, mutation, borrowed/dynamic arrays, derives or foreign aggregate ABI; [contract](data-model.md) |
| Nonrecursive nominal records/enums | Executable bounded C11 profile | Invariant parameters (including phantoms), inferred arguments, generic specialization/forall, constructor catalogs, exhaustive structural matching and imported type identity; independently reconstructed layouts; [manual](nominal-adts.md) |
| Recursive/owning ADTs, functors | Not implemented | Boxing, resource-bearing fields, ownership/drop analysis, recursive layouts and signatures/functors remain absent |
| Cooperative fiber / channel G4 slice | Executable bounded C11 profile | `taskgroup`, eager `launch`/`join`, immediate pre-dispatch internal `cancel`, 1:1 i64 uni channels with bounded FIFO/rendezvous, checked outcomes and atomic `poll` offers with mandatory close arms; limited POSIX `xfiber` direct exec/join/cancel with reaping; MuLIB stackful fibers/channels through one implementation TU; no Plaid integration, poll deadlines, running/blocked internal cancellation, shell streams, async or native workers; [contract](concurrency.md) |
| Plaid/Prolific integration | Not implemented | The cooperative G4 adapter uses MuLIB, not Plaid; profiling and broader concurrency remain open |

## Immediate next iterations

The M2/G3 continuation finishes the already-designed function-scoped defer and
nominal result propagation contract. Dynamic registrations persist across block
and loop exits, retain copied RC operands, and run before remaining lexical
cleanup on explicit/implicit/error returns. The existing value-region verifier
checks tag domination, initialized payload joins and exact return types across
function boundaries. Next error/resource work remains owning Result fields,
general foreign error adapters, effectful cleanup MIR and independent SDK models;
this is not general exception or cancellation support.

The language-only M1 continuation adds decimal f64 literals with scanner/PPX
parity and a versioned float/trivial-tuple MIR execution path. A later M2
prerequisite opens only the reviewed scalar safe-import profiles (signed/unsigned
integers, `_Bool`, binary32 `float`, binary64 `double`); it adds no product-specific kernel and
does not admit resources. v1 evidence remains signed-integer-only; v2 adds bool/f64
and v3 adds unsigned/f32, checking parameter and result types at declarations and callsites.

A second M1 slice adds the bounded value data model: structural records, closed
tagged sums with exhaustive `match`, fixed arrays and owning snapshot slices,
all by-value C11 with bounds-checked access and terminal located value errors
([contract](data-model.md)). Its remaining blockers are MIR lowering for value
data, nominal/parametric/recursively shaped ADTs, data derives (equality,
ordering, printing) and mutable or borrowed arrays. General foreign ownership,
cleanup/effect descriptors, independent checker support and async lowering
remain open; value snapshots do not establish these.
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
    tuple patterns explicitly flatten scalars. Next: records and
   ADTs. Mutable/resource capture lifetimes require further ownership analysis.
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
