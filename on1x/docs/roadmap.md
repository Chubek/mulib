# On1x roadmap and implementation status

## Current feature matrix

| Area | State | Evidence / limits |
|---|---|---|
| Core C++20 compiler | Implemented subset | CMake build, CTest |
| LibStone ASDL/Aurocks integration | Verified P0 subset | Both generated C sources compiled with GCC/Clang; external checkout remains read-only |
| Fallback lexer | Implemented and tested | Bootstraps without LibStone |
| Scanner parity and token lifetime | Verified P0 subset | 244 deterministic byte/offset/error cases; repeated and concurrent scan tests |
| Parser / typed AST | Implemented monomorphic subset | Functions, scopes, control flow, imports |
| Frontend input limits | Implemented and tested | 2 MiB per file; 256-level parsing/HIR limits including lambdas; bounded capture metadata |
| Type inference | P1 monomorphic unification | Omitted ordinary parameter annotations, `_` holes, occurs check, mutual recursion and whole-unit constraints; no generalization/defaulting |
| Structural function types / values | P1 subset | Monomorphic aliases, parameters/results, indirect/postfix calls and lexical resolution; no foreign callbacks |
| Anonymous functions / lexical closures | P1 immutable subset | Escaping scalar/function captures, transitive capture, reference-counted C11 environments; no mutable/resource captures or recursive let |
| C11 AOT code generation | Verified subset | Strict GCC/Clang compile/run tests; ASan/UBSan also cover generated C |
| Signed i64 checking | Implemented | Overflow/invalid division tests |
| Native ABI externs | Explicit raw subset | Exact manually supplied ABI required |
| JSON resource facade | Experimental, real-kernel tested | Nested views, lexical drops, numeric bounds and injected allocation failures; errors still terminate |
| ExoML contract verifier | Optional tool source | SDK integration needs supplied LibStone checkout |
| ExoML raw scalar adapter emitter | Optional, narrow subset | Signed integers/void only; no semantic-safety claims |
| General C FFI adapters | Not implemented | Needs ABI/error/ownership model and tests |
| Chisel/Wetstone integration | Not implemented | Current checker only ad hoc ownership restrictions |
| Flintstone/Firestone native lowering | Not implemented | C11 is the sole backend |
| ADTs, match, functors, let-polymorphism | Not implemented | Future language features |
| Plaid/Prolific integration | Not implemented | Future concurrency and profiling features |

## Immediate next iterations

1. **P0 baseline verified** — Generator-backed and independent fallback builds,
   exact scanner parity, bounded parsing, strict generated C11 and real MuLIB
   JSON integration pass on the tested x86_64 Linux toolchains. Keep these
   regression gates when evolving syntax. See [verification](../VERIFICATION.md)
   for versions, commands and remaining limits; other targets are not certified.
2. **P1 in progress** — Structural monotypes and first-class monomorphic
   functions now have a runnable C11 path, bounded unification/grounding,
   inferred parameter/result holes, immutable lexical closure conversion and
   dedicated semantic/syntax/ownership tests.
   Next: type schemes and let-polymorphism with value restriction, tuples and
   ADTs. Mutable/resource capture lifetimes require further ownership analysis.
   The semantic HIR remains C++-owned, not a new generated ASDL semantic AST.
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
