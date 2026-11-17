# On1x capability matrix

Inspection baseline: MuLIB `eeb6de585303e1ea0dfd8bf2260328289b04587c`,
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763` at
`on1x/third_party/libstone` (read-only). Linux x86_64; GCC 16.2.1,
Clang 22.1.8. This matrix distinguishes executable subsets from completion of
the general capabilities. Historical evidence remains in `../VERIFICATION.md`.
The two fresh pre-change baseline suites passed (GCC/generated 36/36,
Clang/fallback 35/35). Final continuation commands/results are recorded in
`../VERIFICATION.md`; the full-language mandate is not complete.

The next continuation implemented decimal f64 literals and MIR v2 for floats and
trivially copied tuples. Existing scalar artifacts remain v1. Foreign ownership
and concurrency are not admitted by these slices.

Data-model continuation implemented: structural records, closed tagged sums with
exhaustive elimination, bounded fixed arrays and owning snapshot slices. These
are a by-value C11 profile, not general borrowed/dynamic buffers or foreign ABI.

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Generated/fallback scanning | `grammar/On1x.g`, `src/lexer*.cpp` | Bounded token callback | Functional syntax plus complete decimal float tokens | `test_scanner.cpp`, `scanner_parity.py` | EXECUTABLE | Broader malformed-input/target certification |
| Checked i64 | `src/{parser,infer,sema,codegen}.cpp` | Checked C11 runtime operations | i64 literals/operators | `test_compiler.py`, `test_mir.py` | EXECUTABLE | Broader target conformance |
| f64 values | `src/{type,parser,infer,sema,codegen,primitives}.cpp`, `runtime/on1x_runtime.*` | Linux x86_64 binary64; guarded raw double ABI; bit transport/hex output | Monomorphic arithmetic/comparisons, explicit conversions, tuples/closures/staging | `test_f64.py`, `test_meta.cpp`, `test_types.cpp` | EXECUTABLE | f32, independent checkers, generated FFI adapters and other targets |
| Float source literals | `src/{numeric,parser,lexer_fallback,ppx_ast,ppx_schema}.cpp`, `grammar/` | Exact binary64 bit emission; 1024-byte literals | Decimal fractions/exponents, additive FloatLiteral AST/PPX | `test_f64.py`, `test_meta.cpp`, `scanner_parity.py` | EXECUTABLE | Hex floats, suffixes, broader compiler/host certification |
| f32 / fixed-width words / pointer-sized integers | `include/on1x/type.hpp` | None | None | None | ABSENT | Exact numeric semantics and ABI |
| Functions / closures | `src/{infer,sema,codegen}.cpp` | Single-thread RC environments | Structural functions, immutable captures | `test_functions.py`, `test_closures.py` | EXECUTABLE | Mutable/resource capture lifetimes |
| Tuples / irrefutable patterns | `src/{parser,infer,sema,codegen}.cpp` | Internal C11 by-value records | Shared name/tuple patterns | `test_tuples.py`, `test_parameter_patterns.py` | EXECUTABLE | Foreign aggregate ABI; refutable patterns |
| Polymorphism | `src/infer.cpp` | Bounded whole-unit specializations/bundles | Restricted schemes / forall | `test_poly*.py` | PARTIAL | General parameterized data types |
| Records / ADTs / exhaustive matching | `src/{parser,infer,sema,codegen}.cpp`, `src/{type,type_safety}.cpp` | Private versioned C11 structs; `ON1X_VALUE_DATA_ABI_VERSION` guard | Structural labeled records; closed sums with `variant`/exhaustive `match`; unit record payload; no guards/recursive/nominal/parametric forms | `test_data.py`, `examples/data_model.on1x` | EXECUTABLE | Nominal recursion, type parameters, derives, data equality/printing, MIR lowering |
| Arrays / slices | `src/{parser,infer,sema,codegen}.cpp`, `runtime/on1x_runtime.*` | By-value fixed arrays; owning snapshot slices with composed offsets; bounds runtime with located exit-70 errors | `array<T,N>`/`slice<T,N>` (extent <= 256), `array[...]`, `a[i]`, `a[]`, `a[lo:hi]`, slice-of-slice; no mutation/borrowing | `test_data.py`, `examples/data_model.on1x` | EXECUTABLE | Mutable arrays, zero-copy borrows, dynamic buffers, foreign ABI |
| Modules | `src/loader.cpp` | Whole-unit C11 | Namespaces, imports, aliases | `test_modules.py`, `test_loader.cpp` | PARTIAL | Interfaces, separate compilation, signatures/functors |
| Macros / PPX | `src/{macros,macro_rules,ppx,ppx_ast}.cpp` | Build-time processes; bounded wire format | Hygienic macros, external PPX | `test_macros.py`, `test_ppx.py`, `test_local_syntax.py` | EXECUTABLE | Process sandboxing; richer data derives |
| Staging | `src/staging.cpp` | AOT closure thunks | Closed code, quote/splice/lift/run | `test_staging.py` | EXECUTABLE | Broader staged data types |
| Managed data / text | `src/primitives.cpp`, `runtime/on1x_runtime.c` | RC sexpr/buffer; bounded text snapshots | Primitive managed types | `test_selfhost.py` | PARTIAL | Owning Unicode text / binary slices |
| Ownership / recoverable errors | `src/{sema,safety}.cpp`, `runtime/` | Lexical JSON/RC cleanup; terminal failures | Restricted resource flows | `test_json.py`, `test_safety.*` | PARTIAL | General moves/borrows, Result/Option, cleanup edges |
| Generic Option / Result | `stdlib/base/{option,result}.on1x` | Existing facade encodings only | No native tagged generic data types | Existing library tests | PARTIAL | Native parameterized ADTs, construction/matching/drop |
| Exceptions / effect system | `src/backend/mir.cpp`, `runtime/` | Scalar effect chains; no unwind edges | No recoverable exceptions or inferred effect summaries | Scalar MIR tests only | PARTIAL | Typed failure/control-flow/cleanup model |
| Raw C FFI | `src/codegen.cpp`, `tools/on1x-bind.cpp` | Exact declared scalar escape hatch | extern; explicit parameter flattening | `test_contracts*.py`, `test_parameter_patterns.py` | PARTIAL | Rich ABI, callbacks and resources |
| Safe foreign admission | `tools/on1x-bind.cpp`, `contracts-exoml/` | Deliberately closed | No admitted safe imports | `test_contracts_sdk.py` | PARTIAL | Independent review plus ABI/use-site/runtime gates |
| Native kernel planning | `std/mulib/`, `tools/on1x-exotract.py` | One implementation TU plus companions | Metadata, raw adapters | `test_mulib.py`, `test_exotract.py` | EXECUTABLE | Not general safe language bindings |
| MIR / reference evaluator | `src/backend/{mir,lower,serialize,evaluate,emit_c}.cpp`, `floating.hpp` | Scalar v1 plus numeric/product v2; exact bits, typed values and private C11 tuples | Optional source/saved MIR, float operations, trivial tuple destructuring/calls/slots/phis | `test_mir.*`, `test_f64.py` | PARTIAL | Managed aggregates, records/ADTs/arrays, ownership, cleanup and async effects |
| Trivial tuple MIR | `src/backend/{mir,lower,serialize,evaluate,emit_c}.cpp` | By-value copies; no owned/borrowed fields; bounded recursive evaluator storage | Nested tuples with i64/bool/f64 leaves; shared source patterns | `test_mir.*` | EXECUTABLE | Resource-bearing aggregates, foreign layouts, separate compilation |
| MIR passes / relational analyses | `src/backend/{passes,relational}.cpp` | Conservative v1/v2 passes; no float folding; Novostone v1 only | Explicit opt-in, versioned pass descriptors | `test_passes.*`, `test_relational.*`, `test_mir.py` | PARTIAL | v2 relational mapping, full-language legality and ownership descriptors |
| C11 AOT | `src/codegen.cpp`, `runtime/` | Standalone C11 for admitted subset | Default backend | Compiler/native suite | EXECUTABLE | Broader language/runtime representation |
| Native / VM backend | `BACKEND.md` | None | None | None | DESIGNED | Real equivalent code generator |
| Async / structured concurrency | `AGENTS.md` | No On1x runtime/lowering | None | None | DESIGNED | Effectful MIR, task ownership/state machines |
| Channels / select / actors / workers | MuLIB headers; LibStone Plaid | Independent facilities, not On1x semantics | None | No source-language gate | ABSENT | Checked language/runtime adapter |
| Diagnostics / repair | `src/diagnostics.cpp` | Optional Syren build-time adapter | Located errors; proposed edits only | `test_diagnostics.*` | PARTIAL | Full stable diagnostic taxonomy |
| Formatter / LSP / linter / packages | `tools/` | Build helper only | None for these tools | Existing installation tests | ABSENT | Implement tools and protocol guarantees |
| Standard library | `stdlib/`, `std/` | Persistent helpers, trusted scalar bridges | Partial functional modules | `test_libraries.py`, `test_stdlib_extensions.py` | PARTIAL | Coherent data/resource model and conformance |
| Self-hosting | `stdlib/{sexpr,ppx,visitor}/` | C11 native authoring tools | Compiler remains C++20 | `test_selfhost.py` | PARTIAL | Real compiler-stage migration/bootstrap |

No row is newly certified VERIFIED by source inspection alone. Optional SDKs,
cross-target ABI and TSAN are not implied by default-build tests.
