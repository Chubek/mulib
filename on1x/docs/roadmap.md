# On1x roadmap and implementation status

## Current feature matrix

| Area | State | Evidence / limits |
|---|---|---|
| Core C++20 compiler | Implemented subset | CMake build, CTest |
| LibStone ASDL/Aurocks integration | Verified P0 subset | Both generated C sources compiled with GCC/Clang; external checkout remains read-only |
| Fallback lexer | Implemented and tested | Bootstraps without LibStone |
| Scanner parity and token lifetime | Verified P0 subset | Deterministic byte/offset/error corpus, including macro/PPX/staging/dotted syntax; repeated and concurrent scan tests |
| Parser / typed AST | Implemented functional subset | Functions, tuples/patterns, explicit universal types, scopes, control flow, imports |
| Frontend input limits | Implemented and tested | 2 MiB per file; 256-level parsing/HIR limits including lambdas; bounded capture metadata |
| Type inference | P1 schemes and explicit universals | Global recursive-component and value-restricted immutable let/pattern generalization, rigid `forall` parameters, bounded C11 specialization/bundles; no defaulting or inferred polymorphic recursion |
| Structural function types / values | P1 subset | Monomorphic aliases, parameters/results, indirect/postfix calls and lexical resolution; no foreign callbacks |
| Anonymous functions / lexical closures | P1 immutable subset | Escaping scalar/function/tuple captures, transitive capture, reference-counted C11 environments; no mutable/resource captures or recursive let |
| Tuples / destructuring | P1 product subset | Structural products, nested immutable local/function/lambda patterns, leaf schemes and function/universal-field ownership; no JSON elements or aggregate extern ABI |
| C11 AOT code generation | Verified subset | Strict GCC/Clang compile/run tests; ASan/UBSan also cover generated C |
| Signed i64 checking | Implemented | Overflow/invalid division tests |
| Native ABI externs | Explicit raw subset | Exact scalar ABI; explicit nested parameter patterns flatten left-to-right, no aggregate layout inference |
| Typed hygienic macros | Recursive R7RS-style pattern/template family | Context marks, typed captures, wildcard/literal keywords, list/vector/tuple/AST patterns, dotted tails, arbitrary/nested ellipses and template escapes; top-level definitions, not a full Scheme macro binding system |
| External AST PPX | Core implemented | Source opt-in / CLI registration, bounded shell-free POSIX transport, S-expression schema validation, labeled carriers and namespaced AST extensions |
| PPX authoring / standard processors | Native self-hosted libraries/tools | On1x codec/visitor/schema/PPX libraries and six C11-native processors, with Python bootstrap references; compiler remains C++ |
| Multi-stage programming (MSP) | Typed closed-code AOT | `code<T>`, quote/splice/lift/contextual run, quotation-world checks, reference-counted factories/thunks; no JIT or runtime source eval |
| Native authoring data/I/O | Bounded C11 substrate | Persistent managed `sexpr`, shared `buffer`, byte operations/I/O; process-lifetime bounded text snapshots, not a general String/ADT/module library |
| JSON resource facade | Experimental, real-kernel tested | Nested views, lexical drops, numeric bounds and injected allocation failures; errors still terminate |
| ExoML contract verifier | Optional tool source | SDK integration needs supplied LibStone checkout |
| ExoML raw scalar adapter emitter | Optional, narrow subset | Signed integers/void only; no semantic-safety claims |
| General C FFI adapters | Not implemented | Needs ABI/error/ownership model and tests |
| Chisel/Wetstone integration | Not implemented | Current checker only ad hoc ownership restrictions |
| Flintstone/Firestone native lowering | Not implemented | C11 is the sole backend |
| ADTs, match, functors | Not implemented | Future language features |
| Plaid/Prolific integration | Not implemented | Future concurrency and profiling features |

## Immediate next iterations

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
    includes local syntax bindings, general modules/ADTs, IPC and reviewed processor
    integrations. Recursive patterns, native authoring tools and typed AOT MSP
    now have executable implementations; see [the manual](metaprogramming.md).
3. **P2** — Generate exact target-checked C11 wrapper adapters for reviewed
   pointer-based MuLIB JSON/Vec/Parsec contracts; add `Result`/`Option`
   safeties and explicit success/failure transfers.
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
