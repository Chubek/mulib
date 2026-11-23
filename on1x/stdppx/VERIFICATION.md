# Verification report

## InlineC11 — 2026-11-21

On x86-64 Linux, at MuLIB `b5b4f045` with LibStone `248ad177` and Cephyr
disabled, `ctest --test-dir build/A-reference -R '^on1x.inline_c11$'
--output-on-failure` passed (1/1, GCC 16.2.1). The same
`on1x/tests/test_inline_c11.py` suite passed with Clang 22.1.8 and with Clang
ASan+UBSan on both the processor and the linked client. The suite exercises
real nested calls, operand order, result values, name collisions, malformed
carriers, raw argument types, C compiler diagnostics, limits, authorization and
exclusive companion creation. The standalone stdppx unittest suite passed
(30/30). Cross-compilation was not tested.

## GrammarEmbed continuation — 2026-11-18

Native `GrammarEmbed` uses Firestone `liblr` at configured revision
`934b6ed029bfea28bbf23b5dc77c476154b0dd0d` to emit ordinary On1x NFA/DFA
lexers and LR1/LALR1/LL1 parser engines. GCC/generated, Clang/fallback and
Clang ASan/UBSan/leak-detection integration/bridge/install selections pass
**3/3 each** (including targeted integration reruns). Each integration profile
checks **4,686** short-input results against independent liblr C++ runtimes.
Additional cases cover concrete trees, byte spans, epsilon, conflicts,
left recursion, token helpers, limits, fresh names and LuaEmbed → GrammarEmbed.
The shipped example runs after name-based discovery and installed relocation.

See [`../VERIFICATION.md`](../VERIFICATION.md#grammarembed--native-liblr-lexerparser-emission)
for commands and exact scope, and the [GrammarEmbed guide](grammarembed/README.md)
for the grammar subset and emitted API.

## Native extension integration — 2026-11-15

All **15 native processors** build and have compiler-generated C11 execution
tests. This includes the newly supplied `assertions`, `cond`, `format`, `logging`,
`state_machine`, `tap` and `trace`, now implementing the common Python API as
well as their documented compatibility forms. The **24 Python processors** and
**28 Python protocol tests** remain enabled. See the latest full matrix in
[`../VERIFICATION.md`](../VERIFICATION.md).

The new regressions exercise:

- Native/Python stdout/stderr behavior, inclusive single-evaluation assertions,
  explicit operand/effect order, mutable callbacks and generated-name hygiene.
- Lazy conditional arms, formatted strings/braces, state transition keys,
  duplicate transition rejection and function-entry tracing.
- Compatibility `choose`, `concat`, `print`, `set` and `identity` carriers.
- Standalone native protocol input, empty units, malformed trees and unconsumed
  own-label carriers, all with no partial stdout on failure.
- A six-processor composition example under both Python and native plugins;
  post-transform compiler checks are still required.
- Strict raw-FFI manifest parsing: duplicate review/target fields, unknown fields
  and nonfinite JSON values reject instead of relying on last-key-wins metadata.

CMake builds/installs all fifteen executables. `native/common.on1x` is a shared
helper, not another processor. `make_catalog.py` refreshes the catalog while
preserving design documents unless explicitly asked to regenerate them.
Seventeen design-only processors remain unimplemented. No syntactic PPX or raw
review flag is a foreign ABI, ownership or semantic safety certificate.

The previous eight-native report below is historical and retains its original
test counts and measured snapshot.

## Integrated compiler verification — 2026-11-15

All **24 Python processors** now transform actual compiler ASTs and produce
strict C11 programs with checked execution results. All **eight native PPXs**
build; native self-hosting tests cover the original six and differential compiler
output checks cover `control`, `pipe` and `compose`.

The complete suites pass in `/tmp/opencode/on1x-loader-gcc` (**34/34**),
`on1x-loader-clang` (**33/33**, fallback scanner) and `on1x-loader-sanitize`
(**34/34**, Clang ASan/UBSan). See [configuration and commands](../VERIFICATION.md).

Coverage includes:

- **27 Python protocol tests** in `on1x.stdppx_protocol`, canonical v1 schema parity,
  malformed input/output and subprocess failure behavior.
- `on1x.stdppx` compiler reconstruction, type checking and execution for every
  processor, plus all four shipped On1x examples through the public build helper.
- Ordered single evaluation, mutable `tap` callbacks, `assertions` range checks,
  native/Python `while`/`until`, bounded nested unroll and rejected invalid carriers.
- Timing adapter linkage and exactly one timing report on each tested early-return
  path, including both branches of a void-returning function; trace/log effects.
  Eight injected clock cases cover zero, exact `INT64_MAX`, one-nanosecond
  overflow, excessive/negative seconds or nanoseconds, and clock failure.
- Explicit standard-processor lookup, source authorization through imports,
  mixed `--ppx`/`--stdppx` ordering, namespaced references and relocated installation.
- Existing `on1x.ppx` and `on1x.selfhost` schema, process, hygiene, timeout/size,
  native/reference and generated-C regressions.

The sanitizer build instruments native processors, compiler/runtime/adapters and
generated C11 applications. PPX remains pre-inference; the seventeen design-only
catalog entries remain unimplemented, and raw FFI fixtures grant no safety proof.

## Artifact-production baseline (historical)

- `python -m compileall -q .` — passed.
- `python -m unittest discover -s tests -q` — 24 tests passed, including
  negative cases and real subprocess wire-protocol execution.
- `CC=gcc ./tests/test_timing_c.sh` — passed (GCC C11, `-Wall -Wextra
  -Wpedantic -Werror`).
- `CC=clang ./tests/test_timing_c.sh` — passed (Clang C11, same warning policy).
- JSON catalog consistency and all declared source paths — passed.

### Not executed in that earlier environment

- No `on1xc` binary/whole MuLIB checkout available in this artifact runtime.
- Native `.on1x` implementations have **not** been compiled against current
  frontend versions or differential-tested with the Python programs.
- No C11 program generated by `on1xc` was run here. Python AST schema acceptance
  alone does not establish source AST reconstruction and compiler acceptance.
- Sanitizer instrumentation of generated C11 programs is pending integration.

### Integration gates recorded at artifact production

1. Run every `examples/*.on1x` through the current compiler with explicit
   source PPX opt-in and compare emitted C11 execution with expected results.
2. Build and execute the six original native PPXs and experimental `native/pipe`
   and `native/compose`, comparing their output trees against the Python versions.
3. Check compilation and linking of `timing` with the C11 adapter, and validate
   its clock/return-value behavior on early returns and nested scope branches.
4. Run negative tests for malformed PPX schema, unconsumed carriers, forbidden
   FFI, missing reviewed contracts, unsafe effect/ownership patterns, deep wire
   trees, and symlink escapes in embedded source files.
5. Inspect external processors as trusted executable code; protocol validation
   is not sandboxing.
## LuaEmbed continuation

Native-only `LuaEmbed` and the embedded `llembed` module are tested on Linux
x86-64 with GCC 16.2.1/generated scanner and Clang 22.1.8/fallback scanner.
Final focused selections pass **5/5** (GCC), **4/4** (Clang), and **6/6**
(Clang ASan/UBSan/leak detection), including actual On1x C11 programs,
metaprogram chains, 128 allocation-failure samples and relocated installation.
Standalone Python/schema regressions remain **30/30**.

The parent full generated-scanner run is **63/65**, with only the known
Elvi-scrape manual fingerprint failures. A strict-clean C++ compiler is not
claimed: preexisting loader indentation warnings remain. New C11 kernel,
bridge, processor and client code is compiled with warnings-as-errors.
See [`../VERIFICATION.md`](../VERIFICATION.md#luaembed-native-ppx-and-embedded-llembed)
for revisions, exact commands, supported scope and limits, and the
[LuaEmbed contract](luaembed/README.md).
