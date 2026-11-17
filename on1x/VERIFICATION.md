# On1x verification — 2026-11-17

## Language continuation — bounded value data model (records, sums, arrays, slices)

Scoped implementation task: executable structural records, closed tagged sums
with exhaustive `match`, fixed arrays and **owning snapshot slices** in the
canonical frontend and by-value C11 backend. This is an M1 slice; MIR lowering,
mutable/borrowed arrays, nominal/parametric ADTs, derives, ownership and safe
FFI remain open. The value-data contract is documented in
`docs/data-model.md`; syntax/semantics in `docs/language.md`.

Re-inspected base: MuLIB `eeb6de585303e1ea0dfd8bf2260328289b04587c`, configured
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763` at
`on1x/third_party/libstone`, unchanged. Linux `7.2.6-arch2-1` x86_64 GNU/Linux;
GCC/g++ 16.2.1 (20260810), Clang/clang++ 22.1.8, CMake 4.4.3, Python 3.14.7.

### Pre-change baselines (recorded before the slice)

- `TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-loader-gcc
  --output-on-failure --output-junit baseline-data.xml` → 37/37
  (GCC/generated scanner).
- `TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-loader-clang
  --output-on-failure --output-junit baseline-data.xml` → 36/36
  (Clang/fallback scanner; no scanner-parity test in the fallback tree).
- `git diff --check` clean; LibStone checkout unchanged.

### Implementation

- New syntax, both scanners: `record{label:type,...}` (labels canonicalized
  lexicographically for type identity; constructor operands stay in source
  order), `sum{Tag:type,...}`, `variant<sum{...}>(Tag, payload)`,
  `match(expr){Tag(pattern)=>arm,...}` (exhaustive, one arm per constructor),
  `array<T,N>`/`slice<T,N>` (extent ≤ 256), `array[...]`, checked `a[i]`,
  length `a[]` and bounds-checked `a[lo:hi]` slice construction. No lexical
  changes; the fallback scanner accepts identical text.
- New structural types `Record`/`Sum`/`Array`/`Slice` in `include/on1x/type.hpp`
  with label/extent shape data, canonical `Type::data` construction and
  `type_name` printing; inference (`src/infer.cpp`) builds canonical data
  terms, unifies labels/extent, infers array element types/extents, checks
  variant payloads and exhaustive duplicate-free match arms against known sum
  shapes.
- Frontend checks (`src/sema.cpp`): value-data positions require trivially
  copied types (i64/f64/bool/tuples/value-data; functions/strings/JSON/sexpr/
  buffer rejected), bounded expansion (256 fields/extent per level, 32 nesting
  levels, 4096-unit width budget), data-valued extern ABI rejected, record
  field/label arity, unknown labels/constructors, duplicate arms/fields, arm
  payload and result agreement, match-result width. The tuple-annotation
  `record{a:i64}` ordering is shape-canonical.
- C11 codegen (`src/codegen.cpp`): private compiler-owned structs
  (`on1x_data_type_N`), sums as `int64_t tag` + payload union, arrays as fixed
  element storage, slices as backing copy + `start`/`length`; field/index/
  slice/length/match/variant lowering with source-located bounds checks
  (`on1x_bounds_index`, `on1x_bounds_slice`) and discriminant validation
  (`on1x_invalid_tag`); `ON1X_VALUE_DATA_ABI_VERSION` guard emitted when used;
  slices avoid intermediate aggregate temps so generated code compiles with
  `-Wall -Wextra -Wpedantic -Werror` for both host compilers.
- Runtime (`runtime/on1x_runtime.[ch]`): located value errors, exit 70
  (bounds) — same terminal-failure policy as checked arithmetic; no new
  allocation, refcounting or destructor paths.
- Fail-closed adapters: `--emit-ir` rejects value data with
  `ON1X-MIR-UNSUPPORTED ... scalar MIR cannot lower record{...}`;
  `src/type_safety.cpp`, `src/safety.cpp`, `src/backend/lower.cpp` and
  `src/staging.cpp` reject the new kinds rather than approximating them.
  PPX round-trip support added instead: `ValueDataType`/`ValueData` nodes in
  `PPX.schema.json` (canonical, standalone and stdlib copies), `src/ppx_ast.cpp`
  read/emit, `DataExpression` macro category, signature writers
  (`stdlib/ppx/signature.on1x`, `stdppx/{passes.py,_support/passes.py}`), and
  `--types` listing.
- Not implemented: MIR for value data, data equality/ordering/printing, field
  or element mutation, zero-copy borrows, dynamic arrays, guards or refutable
  patterns, recursive/nominal/parametric data types, derives, foreign
  aggregate ABI.

### Verification evidence

- `TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-loader-gcc
  --output-on-failure --parallel 3 --output-junit results-data.xml` →
  **38/38**, including the new `on1x.data` suite.
- `TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-loader-clang
  --output-on-failure --parallel 3 --output-junit results-data.xml` →
  **37/37** after reconfiguration (fallback-scanner profile).
- `on1x.data` covers: record projection/nesting/canonical labels, sums with
  record/tuple/record payloads, arrays/slices of records, slice-of-slice
  composition, evaluation order, value passthrough; runtime bounds
  rejections (index high/negative, slice end/reversed/negative → exit 70,
  located message); static rejections (non-exhaustive, duplicate/unknown
  arms, arm arity/payload/result mismatch, unknown field, field on sum,
  duplicate fields, managed fields/results, data extern ABI, wrong variant
  payload/unknown tag, index on record, mixed array elements); MIR and
  structural-adapter fail-closed paths; deterministic `--emit-c`; and an
  end-to-end `tools/on1x-build.py` run.
- `examples/data_model.on1x` executes via the build helper with expected
  output (`0x1.9000000000000p+4`, `0x1.9000000000000p+4`,
  `0x0.0000000000000p+0`, `50`, `4`, `40`) on both compiler configurations,
  and identically under GCC `-O3` and Clang `-O2`.
- Sanitizers: fresh ASan/UBSan tree (`cmake -S on1x -B
  /tmp/opencode/on1x-data-sanitize -DCMAKE_C_COMPILER=clang
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_FLAGS="-fsanitize=address,undefined
  -fno-omit-frame-pointer" -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined
  -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
  -DBUILD_TESTING=ON`); full suite `TMPDIR=/tmp/opencode ctest --test-dir
  /tmp/opencode/on1x-data-sanitize --output-on-failure --parallel 3
  --output-junit results-data.xml` → **38/38**, including `on1x.data` (the
  compiler and generated code run under ASan/UBSan; no leak/UB findings).
- Optional-SDK profile (ExoML/Exotract/Chisel/Nerotypical/Novostone enabled,
  `ON1X_SATIE_ROOT=/tmp/opencode/on1x-final-type-sdk`, configuration mirroring
  the historical SDK tree): independent checkers fail closed on data programs
  with located diagnostics — `--check-memory` →
  `ON1X-MEMORY: expression memory model unavailable`, `--check-types` →
  `ON1X-TYPE-SAFETY: ... outside the structural adapter`, `--emit-ir` and
  `--analyze-ir` → `ON1X-MIR-UNSUPPORTED: scalar MIR cannot lower sum{...}`;
  full SDK suite `TMPDIR=/tmp/opencode ctest --test-dir
  /tmp/opencode/on1x-data-sdk --output-on-failure --parallel 3
  --output-junit results-data.xml` → **46/46**, including `on1x.data` and the
  SDK-specific suites (`type_safety`, `safety`, `external_memory_model`,
  `relational_hir`, `exotract`, `contracts_sdk`).
- PPX/AST wire compatibility: `--emit-ast` serializes value data as
  `ValueData`/`ValueDataType`; an external Python identity PPX round-trips
  `data_smoke.on1x` through `--ppx` and the rebuilt program reproduces the
  direct-compile output byte for byte. `--types` prints canonical data shapes
  (e.g. `take : fn(record{a:i64, b:i64}) -> i64`).
- Staged code over value data works: `quote { record{x:1} }` yields
  `code<record{x:i64}>` whose `run` returns the record; projections after
  `run` behave like ordinary values.

### Remaining blockers

MIR v2 for value data, mutation/borrowing, nominal/parametric ADTs, derives,
ownership resources, recoverable errors, safe FFI, concurrency.

## Language continuation — decimal literals and numeric/product MIR v2

Scoped implementation task: extend the existing frontend and MIR with decimal
f64 literals, dedicated floating operations and **trivially copied** nested
tuples. This is an M1/M3 prerequisite slice, not full aggregates, ownership,
safe FFI or concurrency. The earlier value-only results below are historical;
their float-literal/MIR exclusions are superseded only by this scoped profile.

Re-inspected base: MuLIB `eeb6de585303e1ea0dfd8bf2260328289b04587c`, configured
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763` at
`on1x/third_party/libstone`, unchanged. Linux `7.2.6-arch2-1` x86_64 GNU/Linux;
GCC/g++ 16.2.1 (20260810), Clang/clang++ 22.1.8, CMake 4.4.3, Python 3.14.7.

### Compatibility and implementation

- Decimal fraction/exponent tokens work in both Aurocks and fallback scanners.
  The bounded source decoder rejects overflow/underflow-to-zero and emits exact
  binary64 bits. Existing i64 literals, including hexadecimal spellings with
  `e` digits, are unchanged. Float signs remain unary operators.
- The v1 PPX schema gains `FloatLiteral` under `NumericLiteral`, not
  `IntegerLiteral`; all three validator implementations and the standalone
  schema copy are synchronized. Native/Python identity, deriving, folding and
  malformed-spelling protocol tests exercise the new node. Old pinned PPXs may
  reject the additive constructor and must be rebuilt deliberately.
- Existing scalar source still produces MIR v1; saved v1 modules continue to
  round-trip. Numeric/product source selects v2, with explicit float opcodes,
  canonical IEEE constant bits and nested structural tuple type descriptors.
  Bad versions, types, tuple arities/indices, NaN constants, effects and entry
  signatures reject rather than produce C or overwrite existing output.
- Nested tuples admit only i64/bool/f64 leaves in MIR. Calls, returns, slots,
  recursive functions, shared source destructuring patterns and simultaneous
  phi transfers use value copies. The typed evaluator API exposes exact float
  bits and ordered tuple elements; it validates incoming shapes and NaNs.
  Frame budgets count nested fields and copies consume work. Type admission and
  memoized structural equality share the verifier budget.
- Floating arithmetic/conversions/comparisons retain environment/range-check
  effects. No float arithmetic folding is added. Pass descriptors are version 2;
  reports identify `portable-numeric-product-v2` while preserving the v1 report
  envelope and the legacy scalar target name. Novostone mapping remains v1-only
  and explicitly rejects v2; optional type/ownership adapters still reject f64.
- A final review caught an evaluator guard that originally checked SSE2 presence
  but not `FLT_EVAL_METHOD`. The GCC regression now recompiles the **actual**
  evaluator translation unit with `-mfpmath=387`, links it with the canonical
  frontend and requires a located unsupported error. This closes silent x87
  double-rounding, without substituting a test evaluator or changing scalar code.

### Validation commands

Final full-suite results after the x87 guard regression: **155/155 CTests**,
with no test exclusions, disabled tests or skips:

| Build directory under `/tmp/opencode/` | Profile | Result |
|---|---|---|
| `on1x-loader-gcc` | Strict GCC Debug / generated scanner | 37/37 |
| `on1x-loader-clang` | Strict Clang Debug / fallback scanner | 36/36 |
| `on1x-loader-sanitize` | Clang / generated scanner / ASan+UBSan | 37/37 |
| `on1x-final-gcc` | GCC / generated scanner / optional SDKs ON | 45/45 |

The JUnit files are `results-v2-final.xml` in those four directories. Earlier
`results-v2-first.xml` runs also passed before the additional boundary tests.

The four existing strict Debug profiles are rebuilt and tested without excluding
tests. The sanitizer cache instruments compiler, runtime and emitted C with
ASan/UBSan and nonrecovering checks. Native PPXs are ON in all four. The optional
SDK configuration uses ExoML extraction, Exotract, Chisel, Nerotypical and
Novostone SOURCE with the existing prepared Satie SDK; Syren is OFF. SDK passing
tests are regression/admission-rejection evidence, not numeric safety proofs.

```sh
cmake --build /tmp/opencode/on1x-loader-gcc --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-loader-gcc \
  --output-on-failure --parallel 3 --output-junit results-v2-final.xml
cmake --build /tmp/opencode/on1x-loader-clang --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-loader-clang \
  --output-on-failure --parallel 3 --output-junit results-v2-final.xml
cmake --build /tmp/opencode/on1x-loader-sanitize --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-loader-sanitize \
  --output-on-failure --parallel 3 --output-junit results-v2-final.xml
cmake --build /tmp/opencode/on1x-final-gcc --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-final-gcc \
  --output-on-failure --parallel 3 --output-junit results-v2-final.xml
```

`test_mir.py` compares HIR C11, plain/optimized MIR C11 and plain/optimized
reference evaluation. Fixtures include nested mutable tuple snapshots,
destructured parameters, recursion, generic imported numeric tuples, output
order/short-circuiting, NaNs/infinities, signed zero, subnormals, maximum finite
values, conversion traps, saved-wire round-trips and independently authored
aggregate phi permutations. `test_f64.py` now executes its independent Python
bit/arithmetic oracle in **all five** paths, and adds decimal rounding cases
including halfway ties. MIR tests also reject invalid floating environments and
exhaust aggregate frame storage under sanitizers. Scanner parity includes
complete/malformed exponent and dotted/ellipsis spellings plus seeded byte fuzz.

Additional complete f64/MIR suite runs pass with emitted-code GCC LTO,
Clang's individual precision-relaxation flags (the primitives restore precise
semantics), and explicit float-cast-overflow sanitization:

```sh
python3 on1x/tests/test_f64.py /tmp/opencode/on1x-loader-gcc/on1xc \
  "$PWD/on1x" /tmp/opencode/on1x-v2-lto-float gcc --cflag=-O3 --cflag=-flto
python3 on1x/tests/test_mir.py /tmp/opencode/on1x-loader-gcc/on1xc \
  "$PWD/on1x" /tmp/opencode/on1x-v2-lto-mir gcc --cflag=-O3 --cflag=-flto
python3 on1x/tests/test_f64.py /tmp/opencode/on1x-loader-clang/on1xc \
  "$PWD/on1x" /tmp/opencode/on1x-v2-precise clang \
  --cflag=-fno-honor-nans --cflag=-fno-signed-zeros
python3 on1x/tests/test_mir.py /tmp/opencode/on1x-loader-clang/on1xc \
  "$PWD/on1x" /tmp/opencode/on1x-v2-precise-mir clang \
  --cflag=-fno-honor-nans --cflag=-fno-signed-zeros
python3 on1x/tests/test_f64.py /tmp/opencode/on1x-loader-sanitize/on1xc \
  "$PWD/on1x" /tmp/opencode/on1x-v2-cast-sanitize clang \
  --cflag=-fsanitize=address,undefined,float-cast-overflow \
  --cflag=-fno-omit-frame-pointer --cflag=-fno-sanitize-recover=all
```

The real build-helper program also runs with both C compilers:

```sh
python3 on1x/tools/on1x-build.py on1x/examples/float_tuples.on1x \
  --on1xc /tmp/opencode/on1x-loader-gcc/on1xc --cc gcc \
  --via-mir --optimize-ir -o /tmp/opencode/on1x-float-tuples-gcc
/tmp/opencode/on1x-float-tuples-gcc
ldd /tmp/opencode/on1x-float-tuples-gcc
python3 on1x/tools/on1x-build.py on1x/examples/float_tuples.on1x \
  --on1xc /tmp/opencode/on1x-loader-gcc/on1xc --cc clang \
  --via-mir --optimize-ir -o /tmp/opencode/on1x-float-tuples-clang
/tmp/opencode/on1x-float-tuples-clang
ldd /tmp/opencode/on1x-float-tuples-clang
```

Both executables print `0x1.0000000000000p-1`, `0x1.0000000000000p-3`, `4`
on separate lines and depend only on libc/system loader, not LibStone, C++, Lua
or the On1x compiler. This is C11 AOT, not direct native code generation.

### Remaining unsupported profiles

No records/ADTs/exhaustive matching, arrays/slices, f32/other integer widths,
resource-bearing MIR aggregates, general moves/borrows, recoverable errors,
safe-import admission, callbacks, async/await, tasks/channels/select/actors or
native workers are added. Foreign calls remain unsupported in MIR. No new
32-bit/Windows/macOS/other-architecture, shared-runtime ABI, TSAN or self-hosting
claim is made. Follow `docs/capability-matrix.md`, `docs/numerics.md` and
`docs/backend-mir.md` for exact accepted boundaries and next blockers.

## Earlier M1 continuation — binary64 values (not complete M1)

Inspection/patch base: MuLIB `eeb6de585303e1ea0dfd8bf2260328289b04587c`.
The configured read-only LibStone is
`on1x/third_party/libstone` at `55755485acaded7c4ed804c0946bc8c7363f6763`,
with a clean working tree. Platform: x86_64 Linux `7.2.6-arch2-1`;
GCC/g++ 16.2.1 (20260810), Clang/clang++ 22.1.8, both targeting
`x86_64-pc-linux-gnu`; CMake 4.4.3, Python 3.14.7.

Before implementation, rebuilding and running the complete existing suites gave
**36/36 GCC/generated** and **35/35 Clang/fallback**. The old fallback compiler
rejected `examples/f64.on1x:2:16` with `unknown type: f64`. New tests now execute
that actual On1x program through the normal C11 backend and build helper.

### Final results

All four complete profiles pass; **155/155 CTests across configurations**, no
excluded, disabled or skipped tests:

| Build directory under `/tmp/opencode/` | Configuration | Result |
|---|---|---|
| `on1x-loader-gcc` | GCC Debug, generated scanner, optional analyzers OFF | 37/37 |
| `on1x-loader-clang` | Clang Debug, fallback scanner, optional analyzers OFF | 36/36 |
| `on1x-loader-sanitize` | Clang Debug, generated scanner, compiler/runtime/emitted C ASan/UBSan | 37/37 |
| `on1x-final-gcc` | GCC Debug, generated scanner, ExoML extraction/Exotract/Chisel/Nerotypical/Novostone ON | 45/45 |

These reused the documented strict `-Wall -Wextra -Wpedantic -Werror` C11/C++20
builds. All native PPXs remain ON. Syren is OFF in these profiles. The SDK build
retains the existing prepared Satie source SDK at
`/tmp/opencode/on1x-final-type-sdk`, with Novostone's SOURCE provider. No LibStone
or prepared-SDK source was modified. Optional checkers still **reject f64**;
their green regression tests are not a float ownership/type proof. The SDK
profile also runs actual extraction/archive regressions, not a mock SDK.

Exact final build/test commands (from the MuLIB root):

```sh
cmake --build /tmp/opencode/on1x-loader-gcc --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-loader-gcc \
  --output-on-failure --parallel 3 --output-junit results-numeric-final.xml
cmake --build /tmp/opencode/on1x-loader-clang --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-loader-clang \
  --output-on-failure --parallel 3 --output-junit results-numeric-final.xml
cmake --build /tmp/opencode/on1x-loader-sanitize --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-loader-sanitize \
  --output-on-failure --parallel 3 --output-junit results-numeric-final.xml
cmake --build /tmp/opencode/on1x-final-gcc --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-final-gcc \
  --output-on-failure --parallel 3 --output-junit results-numeric-final.xml
```

Baseline JUnit files are `baseline-numeric.xml` in the two loader build
directories. Final JUnit files are `results-numeric-final.xml` in all four
directories. The first post-patch runs exposed a stale standalone PPX schema
copy, failing identity transport and the schema-equality test. The schema was
synchronized with the additive `F64Type` constructor; **all four complete suites
were rerun**, rather than suppressing those failures.

For a fresh default GCC configuration equivalent to the loader profile:

```sh
cmake -S on1x -B /tmp/opencode/on1x-numeric-new -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  -DON1X_LIBSTONE_ROOT="$PWD/on1x/third_party/libstone" \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror'
cmake --build /tmp/opencode/on1x-numeric-new --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-numeric-new --output-on-failure
```

For fallback select Clang/clang++ and `-DON1X_USE_FALLBACK_SCANNER=ON` in
a separate directory. The measured sanitizer cache uses
`-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all`
in both C/C++ flags, plus the semicolon-separated equivalent in
`ON1X_TEST_C_FLAGS`. SDK setup/options remain documented in
`docs/{contracts,memory-safety,type-safety,backend-analysis}.md`.

### New executed coverage and extra runs

`tests/test_f64.py` compares 480 arithmetic results, 720 comparisons, 114 bit
imports/exports (including signaling/negative NaN canonicalization), 228
classification/sign checks, 72 integer-to-float conversions and seven valid
float-to-integer boundary cases with an independent seeded Python oracle.
It reruns the oracle under C `-O2` and `-O3`. Other tests cover:

- Signed zero, gradual underflow, maximum finite values, overflow, infinities,
  unordered NaNs and exact locale-independent hex output.
- Imports, inference, monomorphic numeric constraints, forall instantiation,
  tuple copies/destructuring, escaping closures, staging, counted lexical
  cleanup and environment-allocation failure.
- Real separately compiled C and C++ double definitions, flattened mixed
  parameter patterns, `_Generic`/C++ prototype checks, deliberate incompatible
  prototypes, observable operand order and short-circuit behavior.
- Five rejected numeric casts, four rejected MXCSR modes, twelve located source
  rejections with output preservation, four MIR-mode refusals, float ABI/version
  guards, missing-SSE2 refusal, and fast/finite-math compile rejection.
- Native/Python signature-PPX execution, source-AST round-trip, deterministic C,
  build-helper execution, and no float guard added to ordinary i64 output.

Additional complete numeric-suite commands also passed:

```sh
python3 on1x/tests/test_f64.py /tmp/opencode/on1x-loader-gcc/on1xc \
  "$PWD/on1x" /tmp/opencode/on1x-f64-lto gcc --cflag=-O3 --cflag=-flto
python3 on1x/tests/test_f64.py /tmp/opencode/on1x-loader-clang/on1xc \
  "$PWD/on1x" /tmp/opencode/on1x-f64-precise clang \
  --cflag=-fno-honor-nans --cflag=-fno-signed-zeros
python3 on1x/tests/test_f64.py /tmp/opencode/on1x-loader-sanitize/on1xc \
  "$PWD/on1x" /tmp/opencode/on1x-f64-cast-sanitize clang \
  --cflag=-fsanitize=address,undefined,float-cast-overflow \
  --cflag=-fno-omit-frame-pointer --cflag=-fno-sanitize-recover=all
python3 on1x/tools/on1x-build.py on1x/examples/f64.on1x \
  --on1xc /tmp/opencode/on1x-loader-gcc/on1xc --cc gcc \
  -o /tmp/opencode/on1x-f64-example
/tmp/opencode/on1x-f64-example
ldd /tmp/opencode/on1x-f64-example
```

The example prints `0x1.5400000000000p+5` (42.5). Its dynamic dependencies are
libc and the system loader only: no compiler, LibStone, Lua, libm or C++ runtime.
The normal `examples/hello.on1x` build/run also passes (prints `21`).

### Limits and next blockers

This is the **executable f64 value profile**, not a complete numeric language
or general FFI. See `docs/numerics.md` and `docs/capability-matrix.md`. Float
literals, f32, other integer widths, float MIR/folding, arrays/slices, foreign
records, generated float adapters, safe imports and recoverable conversion
errors remain unsupported. Safe-export gates stay closed. Other architectures,
32-bit targets, Windows/macOS, shared runtime ABI stability and TSAN are not
verified by this slice. No new threading facility is added; TSAN is not a float
value/ownership proof. The finite oracle corpus is not universal equivalence.

## ElviScrape hardening and fixture integration

- ElviScrape retains its six-function C11 single-header API and offline `.elvs`
  providers. Extraction now uses bounded iterative scanner work and staged
  results, ignores inert/raw-text markup, and terminates stale heading context.
  Ambiguous Google targets, malformed URL references and request fragments
  reject without loosening HTTPS/private-network policy.
- Independent HTTPS fixtures cover negotiated certificate/hostname/protocol/ALPN
  checks before HTTP writes, partial I/O, fixed/chunked/authenticated-EOF framing,
  malformed/duplicate metadata, unsupported content encoding, cumulative body
  limits, cleanup and unchanged outputs on failure. Per-allocation injection
  exercises compile/load/URL/parse/search/HTTPS paths with no outstanding tracked
  allocations after each attempt.
- **8/8 selected CTests pass with Clang ASan/UBSan**: three ElviScrape tests and
  all five other shared-webscan consumers. The final sanitizer-backed fuzz
  campaign executes **118,189 inputs** across DSL, HTML, all anchor modes and
  URL/redirect invariants, without a sanitizer/assertion failure. This is bounded
  testing, not a proof of complete HTML5 behavior or transport security.
- Both trusted provider files are registered in `std/mulib/catalog.json`.
  Explicit fixture selection now preserves source-pinned recipes, required
  exports and evidence inputs. The complete archive's `shylock-core` C math
  dependency is explicit, including GCC consumers without sanitizer-provided
  link libraries. The optional owned parser remains off.
- The complete `test_mulib.py` suite passes with GCC and Clang ASan/UBSan;
  **the previously recorded ElviScrape fixture-inventory failure is resolved**.
  Both embedded providers compile and drive offline extraction through a test-only,
  ABI-exact scalar adapter in generated On1x clients. The complete
  `test_exotract.py` suite also passes with GCC and Clang ASan/UBSan, including
  separate-TU C and On1x consumers of the one-TU archive.
- Header/manual and 16 scanner/provider/fixture/test evidence inputs are pinned.
  Source semantics/drift tests and real ExoML SDK extraction tests pass.
  `/tmp/opencode/elvi-solid-contracts-final` passes independent pin/check/recheck:
  `gate_b=true`, unchanged inputs, **six actual exports**, and
  `safe_export=false`. Annotations remain unreviewed; no safe FFI, independent
  certificate/DNS validation or Chisel/Wetstone usage proof is implied.

Artifacts: `/tmp/opencode/elvi-solid-build` (sanitizer CTests/fuzzer),
`/tmp/opencode/elvi-solid-{on1x-mulib-final,exotract-final}` (Clang sanitizer
consumers), `/tmp/opencode/elvi-solid-{on1x-mulib-gcc,exotract-gcc}` (GCC), and
`/tmp/opencode/elvi-solid-fuzz-final.log`.
The initial fuzzer launch hit its default RSS guard with the default sanitizer
quarantine; the completed campaigns use a 16 MiB quarantine and finish below
100 MiB reported peak RSS. The final fuzz command is:

```sh
ASAN_OPTIONS=quarantine_size_mb=16:thread_local_quarantine_size_kb=64 \
  /tmp/opencode/elvi-solid-build/mulib_fuzz_elvi_scrape \
  /tmp/opencode/elvi-solid-corpus -max_total_time=60 -max_len=65536 \
  -timeout=5 -rss_limit_mb=8192 -print_final_stats=1 -verbosity=0
```

## Mune contract rebinding

The Lua standard-library continuation is rebound in both contract catalogs:

- `contracts-exoml/mune.exocontract.json` covers all **52** compiler-discovered
  public Mune declarations, with exact anonymous parameter names preserved.
  Gate B passes; review metadata and `safe_export` remain **false**. This is an
  AI-authored source audit, not independent review or generalized safe FFI.
- The source pin covers `include/mulib-mune.h` and `manual/mulib-mune.md`; the
  evidence pins cover `tests/test_mune_runtime.c`,
  `tests/test_mune_runtime_alloc.c` and `tools/check_mune_runtime.py`. All five
  files enter extraction provenance and archive locks, with negative drift tests.
- The full recipe explicitly opts into `MULIB_IMPL_PARSER`, deduplicated with
  other selected profiles, without widening standalone core Parsec. Real
  Exotract discovery/build reports **52 archive exports, zero declaration-only
  Mune APIs, and one `mulib.o` implementation member**.
- `tests/test_contracts.py` passes with GCC and with Clang ASan/UBSan;
  `tests/test_contracts_sdk.py --extract` passes real libclang discovery,
  pin/check/recheck, deterministic snapshots, denied unsafe promotion and drift
  tests for all five profiles.
- The complete `tests/test_exotract.py` suite passes with GCC and Clang ASan/UBSan,
  including Mune's separate-TU C and generated On1x consumers, existing
  core/MicroJS/ElviScrape/YANTLR/Shiraz/stdlib-bridge consumers and rollback tests.
  Mune acceptance exercises tree/VM/JIT library execution, rooted binary strings,
  native userdata/context ownership through coroutine callbacks, retained chunks,
  bundle validation and unchanged outputs on errors.

Artifacts are under `/tmp/opencode/on1x-mune-exotract-tests/mune` and the
corresponding `mune-app` executable. The source/SDK test directories are
`/tmp/opencode/on1x-mune-contract-tests{,-sanitize}` and
`/tmp/opencode/on1x-mune-sdk-tests`; the independently regenerated/rechecked
Mune contract snapshot is `/tmp/opencode/on1x-mune-rebound-contracts`. The GCC
archive matrix is `/tmp/opencode/on1x-mune-exotract-tests-gcc`.
Reproduce the archive suite with:

```sh
python3 on1x/tests/test_exotract.py \
  /tmp/opencode/on1x-final-gcc/libstone-moonstone/exotract/exotract \
  /tmp/opencode/on1x-loader-gcc/on1xc "$PWD/on1x" \
  /tmp/opencode/on1x-mune-exotract-tests-new clang \
  --cflag=-g --cflag=-fsanitize=address,undefined \
  --cflag=-fno-omit-frame-pointer --cflag=-fno-sanitize-recover=all
```

The broader `tests/test_mulib.py` remains blocked at its existing line-30
inventory assertion: `library/elvi-scrape` is absent from the fixture registry.
This rebinding does not repair or suppress that unrelated failure (also recorded
in the Syren continuation below).

## Syren diagnostics and repair continuation

The optional `ON1X_WITH_SYREN=ON` compiler adapter is implemented and tested;
see [diagnostics.md](docs/diagnostics.md). No grammar or runtime changes, SDK
patches, or new stdlib modules are part of this continuation. The actual supplied
LibStone revision is `55755485acaded7c4ed804c0946bc8c7363f6763`, with a clean SDK
working tree.

- `/tmp/opencode/on1x-syren`: GCC 16.2.1 Debug, generated scanner,
  `ON1X_BUILD_NATIVE_PPX=OFF`; **37/38 CTests pass**. The sole failure is
  `on1x.mulib`, `tests/test_mulib.py:30`, the existing fixture-inventory assertion.
  It also fails at the identical assertion in `/tmp/opencode/on1x-no-syren`
  (Syren OFF, fallback scanner); it is not repaired by this compiler-tooling task.
- `/tmp/opencode/on1x-syren-asan`: GCC Debug, fallback scanner, ASan/UBSan,
  non-PIE compiler and generated C11 executables; **35/35 selected CTests pass**
  with `ctest -j 8 -E '^on1x.(mulib|install)$' --output-on-failure`.
  Inventory and installation were excluded from this sanitizer run.
- Both profiles pass `on1x.diagnostics_api` and `on1x.diagnostics`, including
  1,000 deterministic malformed inputs, Unicode/invalid UTF-8 reporting,
  source edits/statuses, whole-import-graph checking, PPX refusal and
  overwrite/symlink protection. A repaired proposal is C11-compiled and executed
  (exit value 42), with sanitizer flags in the sanitizer profile.
- All **36** `examples/**/*.on1x` inputs have identical exit codes and emitted C
  with the generated-scanner Syren build and fallback-scanner non-Syren build.
- `src/diagnostics.cpp` passes an independent
  `-std=c++20 -Wall -Wextra -Wpedantic -Werror -fsyntax-only` check.

These are compiler-valid **proposals**, not proofs of intended behavior, safe FFI
admission, runtime error recovery or automatic adoption of an edited program.
Normal compilation still rejects invalid input; repair writes only a new file.

## Standard-library and native PPX extension integration

**PASS: 107/107 final CTests, with no failures, disabled or skipped tests.**

| Build under `/tmp/opencode/` | Configuration | Final suite |
|---|---|---|
| `on1x-loader-gcc` | GCC 16.2.1, generated scanner | 36/36 |
| `on1x-loader-clang` | Clang 22.1.8, fallback scanner | 35/35 |
| `on1x-loader-sanitize` | Clang 22.1.8, generated scanner, ASan/UBSan | 36/36 |

All three are Debug builds using `-Wall -Wextra -Wpedantic -Werror`. Final JUnit
evidence is `results-extensions-final.xml` in each directory. Initial
`results-extensions.xml` exposed a Clang-only unused helper in the status-only
bridge configuration. Conditional compilation was fixed without suppressing
warnings, then **all three complete suites were rerun**.

### Executed coverage

- Compiler admission of **118 library modules**, six On1x library programs,
  existing native Basis regressions and the new pure/Parsec examples. The Basis
  catalog remains 186 entries / 60 partial bindings; status was not promoted.
- Checked S-expression Option/Result tags, callback short-circuiting, map
  lookup/removal, graph shape rejection and deduplicated cyclic BFS with bounded
  edge/membership work; signed scanning including `INT64_MIN`, rollback on
  overflow, bounds and snapshot-free repeated byte searches.
- The scalar bridge calls **canonical MuLIB** JSON/YAML/TOML/INI, BLAKE2s/BLAKE3
  and POSIX process helpers, with automatic adapter/kernel linkage and one
  kernel implementation TU. Status tuples distinguish invalid data, missing and
  empty INI values, and whole-document rejection after a matching key.
- Counted adapter allocation failures, temporary-copy cleanup, pool exhaustion
  and process-exit cleanup. Each of seven bridge groups and the status-only
  configuration independently compiles under strict flags.
- A local Python TCP server and On1x numeric-IPv4 client exercise offset sends,
  binary hex reception with NUL/0xff, text-NUL rejection, EOF, invalid offsets,
  double close and stale handles after slot reuse. No external network service.
- All **15 native PPXs** build/install and execute compiler-generated C11.
  Newly supplied assertions/cond/format/logging/state_machine/tap/trace have
  Python/native behavior comparisons, operand-order and compatibility-form
  tests, malformed carriers, duplicate transitions and no-output protocol errors.
  A six-processor example runs through both transports.
- All **24 Python PPXs**, **28 Python protocol tests**, strict raw-FFI manifest
  rejection and existing type/resource/staging/macro/MIR/scanner regressions.
- Installation/prefix relocation executes the installed INI consumer against an
  external MuLIB root, in addition to Basis, PPX and `std.mulib` clients.
- Real Exotract/ExoML archive tests pass with GCC, Clang and ASan/UBSan:
  JSON/YAML/TOML/INI/cryptohash/socket discovery/build and an On1x JSON/INI/hash
  bridge consumer without manual adapter selection. Core/MicroJS/YANTLR/Shiraz,
  artifact drift, ABI mismatch and rollback regressions remain enabled.

### Identity and artifacts

MuLIB HEAD: `e67f842f9211f6bbabf35cea07e6c55d66045e52` plus current uncommitted
changes. The supplied read-only LibStone is clean at
`55755485acaded7c4ed804c0946bc8c7363f6763`; it was not updated or patched.
Optional SDK integrations are OFF in the full loader matrix and tested separately
using existing real Exotract executables from
`on1x-final-{gcc,clang-fallback,sanitize}` and the rebuilt loader-matrix compilers.
Archive logs and artifacts are `/tmp/opencode/on1x-extensions-exotract-{gcc,clang,sanitize}.log`
and matching directories. System libclang itself is not instrumented.

### Reproduction

From the MuLIB root:

```sh
cmake -S on1x -B /tmp/opencode/on1x-extensions-new -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror'
cmake --build /tmp/opencode/on1x-extensions-new --parallel 4
TMPDIR=/tmp/opencode ctest --test-dir /tmp/opencode/on1x-extensions-new \
  --output-on-failure --parallel 3 --output-junit /tmp/opencode/on1x-extensions-new/results.xml
TMPDIR=/tmp/opencode python3 on1x/tests/test_exotract.py \
  /tmp/opencode/on1x-final-gcc/libstone-moonstone/exotract/exotract \
  /tmp/opencode/on1x-loader-gcc/on1xc "$PWD/on1x" \
  /tmp/opencode/on1x-extensions-exotract-gcc gcc
```

For Clang fallback use `clang`/`clang++` and `-DON1X_USE_FALLBACK_SCANNER=ON` in
a separate directory. For sanitizer coverage, use Clang generated scanning and
append `-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all`
to both C/C++ flag strings and set
`-DON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-omit-frame-pointer;-fno-sanitize-recover=all`
as one quoted CMake argument. Sanitized archive tests additionally receive
`--cflag=-g --cflag=-fsanitize=address,undefined --cflag=-fno-omit-frame-pointer
--cflag=-fno-sanitize-recover=all`. For a new real extraction SDK build, follow
[`contracts-exotract/README.md`](contracts-exotract/README.md).

These remain trusted raw scalar adapters and bounded syntactic transforms,
**not** independent ExoML review or generalized safe FFI. No f64/record/slice ABI,
foreign-owner lowering, native ADTs, full SML/Base/Batteries conformance, TLS
socket API or completed Shylock backend is claimed. Operation contracts and
examples: [`stdlib/README.md`](stdlib/README.md),
[`stdlib/_bridge/README.md`](stdlib/_bridge/README.md),
[`stdppx/README.md`](stdppx/README.md).

## Earlier artifact-only verification (historical)

The original extension artifact reported the following before local compiler
integration. These statements retain their historical scope, not current status:

- GitHub source/API read of current `Chubek/mulib` plus `janestreet/base/src` and `ocaml-batteries-team/batteries-included/src` directories.
- C bridge tested with GCC and Clang using `-std=c11 -Wall -Wextra -Werror -pedantic -fsyntax-only` **against signature-matching local mock headers**. This proves only C syntax and basic type compatibility of the bridge under these mocks.
- The On1x compiler was not executable locally; no native On1x unit tests or PPX binaries were compiled/run.
- Actual MuLIB archive generation, linkage and ABI/contract validation are pending.
- No claimed complete Base/Batteries parity, no crypto/security certification, and no generalized safe FFI.
