# On1x verification — 2026-11-17

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
