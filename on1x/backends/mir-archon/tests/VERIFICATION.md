# Native differential verification

## Executed results (2026-11-21)

Host: Linux x86_64, kernel `6.18.52-1-lts`; GCC `16.2.1 20260810`;
Clang `22.1.8`. Working-tree changes from the coordinated integration were
tested on MuLIB `94b27511328c61a219c90cf531ca68e952bc2a85` and LibStone
`5d733d69f19fac1d8a05294fe54e4a4ec863fc4c`.

The actual generated backend reports **`AMD64/SysVAMD64`**. Installation:
`/mnt/andromeda/tempdir/opencode/on1x-archon-toolchain/install`.
Combined On1x build:
`/mnt/andromeda/tempdir/opencode/on1x-archon-build`.

| Gate | Result |
|---|---|
| GCC strict C11 / native differential | **58/58 passed**, SDK-native and optimized SDK-native included |
| Clang strict C11 + ASan/UBSan / native differential | **58/58 passed**, same SDK-native paths |
| Independent Python unsigned oracle | **588/588 results matched**, boundary values and seeded random pairs, all four unsigned widths |
| API rejection/cancellation | Passed ordinary and ASan/UBSan; **131 cancellation checkpoints** on the selected loop fixture |
| Actual generated native frame exhaustion | Rejected at stage 3: `scalar GAS stack frame exceeds 64 KiB` |
| Source/MIR/output-publication negatives | Passed; existing output preserved, failed rename temporaries removed |
| Existing On1x MIR/numeric CTests | **7/7 passed** (`mir_hir`, `mir`, `numeric_limits`, `mir_c11`, bridge, tuple phi, scalar ABI regeneration) |
| Existing `test_mir.cpp` fully instrumented frontend | Passed ASan/UBSan |
| Upstream Archon `AMD64NativeGASTests` | **8/8 passed**, actual assembler/linker/execution tests |

Each source case executes seven independently produced executables: direct
bridge assembly, standalone CLI assembly, HIR C11, MIR C11, optimized MIR C11,
SDK-native assembly, and optimized SDK-native assembly. Four hand-authored
MIR CFG cases execute native and MIR C11. All stdout bytes, process exit
statuses, and runtime trap diagnostics agree with the reference evaluator;
native/C11 stderr must agree byte-for-byte. There are **43 trapping cases**.
The seeded unsigned case also checks a separately implemented Python modulo
oracle, rather than relying only on shared compiler/runtime semantics.

The harness always assembles actual Archon `.s` for native executables.
A failed native stage fails the test. It does not replace native output with
C11, a stand-in backend, or interpreter execution.

## Coverage

* `fixtures/control.on1x`: recursive factorial, nonzero `main` return status,
  short-circuit observable calls, branches, checked loop arithmetic,
  `break`/`continue`.
* `fixtures/tuple_slots.on1x`: nested tuple arguments/results, hidden result
  buffer, bool/float/unsigned fields, snapshots retained across mutable slot
  overwrite and repeated calls/loop iterations.
* `fixtures/signed_boundaries.on1x`: signed extremes, negative
  division/remainder, signed comparisons. Separate trap cases preserve the
  actual On1x rejection of both `INT64_MIN / -1` and `INT64_MIN % -1`.
* `NativeFixture.cpp --phi`: flat and nested tuple phi swaps over five
  self-edge iterations; true/false entry edges share a successor but supply
  different tuples; block storage order differs from CFG traversal.
* Generated source fixtures: `u8/u16/u32/u64` arithmetic, unsigned high-bit
  comparisons, wrapping arithmetic, bit operations, shifts and conversions;
  checked signed/unsigned overflow, zero division/remainder, invalid shifts,
  negative unsigned conversion, NaN-to-integer conversion, assertion traps.
  Fixtures explicitly declare expected trap/return outcome in their names;
  an unintended early trap in a positive case cannot count as a pass.
* Both `f32` and `f64`: exact bit carriers, negative zero, smallest subnormal,
  largest subnormal, smallest normal, largest finite, both infinities,
  canonical NaN, arithmetic, all six comparisons, sign/finite/NaN predicates,
  cross-width and integer conversions.
* Malformed version, SSA identity/effect chain, noncanonical NaN, malformed
  serialization and deeply malformed syntax; verifier node/work limits,
  per-function and module instruction limits, values, local bytes and output
  bytes; cancellation at every callback position observed in a complete run.
* Frame limit fixture uses 4096 i64 locals and 4096 constant SSA values:
  both canonical verification and bridge lowering succeed, then the actual
  native backend rejects its aligned frame. Testing 4500 SSA values instead
  hits Archon's 4096-value cap first and does **not** establish frame coverage.

## Reproduction

See [REGISTRATION.md](REGISTRATION.md) for the two CMake/CTest registrations.
The following is the final integrated ordinary run, from repository root.
The fixture tool was built with C++20 and
`-Wall -Wextra -Wpedantic -Werror`, linked to the combined build's bridge,
frontend/scanner archives and the actual installed `libarchon_backend.a`.

```sh
P=/mnt/andromeda/tempdir/opencode
B=$P/on1x-archon-build
python3 on1x/backends/mir-archon/tests/native_differential.py \
  --fixture-tool "$P/on1x-archon-native-fixture" \
  --archon-tool "$B/backends/mir-archon/on1x-archon-mir" \
  --on1xc "$B/on1xc" --sdk-target x86_64-unknown-linux-gnu --cc cc \
  --runtime "$B/libon1x_runtime.a" \
  --scalar-abi "$B/backends/mir-archon/libon1x_archon_scalar_abi.a" \
  --runtime-include "$PWD/on1x/runtime" --work "$P/on1x-archon-differential"
"$P/on1x-archon-native-fixture" --checks
ctest --test-dir "$B" -R 'mir|numeric' --output-on-failure
```

Upstream scalar GAS test command, run from
`on1x/third_party/libstone/cornerstone/Archon`:
`TMPDIR=/mnt/andromeda/tempdir/opencode python3 -m unittest
tests.test_native_scalar_gas.AMD64NativeGASTests`.

Sanitizer build details:

```sh
cmake -S "$P/on1x-archon-toolchain/generated" \
  -B "$P/on1x-archon-native-san-toolchain" -DCMAKE_C_COMPILER=clang \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS='-g -fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build "$P/on1x-archon-native-san-toolchain" --target archon_backend --parallel 4
cmake -S on1x -B "$P/on1x-archon-native-san-frontend" \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS='-g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DON1X_USE_FALLBACK_SCANNER=ON -DON1X_BUILD_EXTENSION_SDK=OFF \
  -DON1X_BUILD_NATIVE_PPX=OFF -DON1X_WITH_SYREN=OFF -DBUILD_TESTING=ON
cmake --build "$P/on1x-archon-native-san-frontend" \
  --target on1x_frontend on1x_mir_tests --parallel 4
"$P/on1x-archon-native-san-frontend/on1x_mir_tests"
```

`NativeFixture.cpp` and `Bridge.cpp` were compiled together with Clang C++20,
strict warnings, `-g -fsanitize=address,undefined -fno-omit-frame-pointer`,
linking those freshly instrumented frontend/scanner/Archon archives, producing
`$P/on1x-archon-native-fixture-san`. The same compilation with `Tool.cpp`
instead of `NativeFixture.cpp` and
`-DON1X_ARCHON_TARGET_TRIPLE="x86_64-unknown-linux-gnu"` produced
`$P/on1x-archon-native-tool-san`.

Run the differential command above with those two `-san` tools, `--cc clang
--sanitize`, and work directory `$P/on1x-archon-differential-san`; run the
sanitized fixture tool's `--checks` too. `--sanitize` recompiles the real
`on1x_runtime.c` and `ScalarABI.c` with strict C11 and ASan/UBSan. The combined
`on1xc` SDK/CLI process remains uninstrumented; its native outputs execute
against the instrumented runtime. Generated assembly itself has no ASan
instrumentation. Frontend/bridge/native-emitter memory lifetime is covered by
the separately fully instrumented fixture/tool path.

Each work directory retains sources, serialized MIR, assembly, reference C,
expected stdout/status/diagnostics, executables and `report.json`. Run logs:
`$P/on1x-archon-differential{,-san}.log` and
`$P/on1x-archon-limits{,-san}.log`.

## Findings and boundaries

* Found a real integration failure while `Tool.cpp` acquired target checking:
  `on1x-archon-mir --print-target` failed with
  `target mismatch: requested , configured `. Its target lacked
  `ON1X_ARCHON_TARGET_TRIPLE`. Reported with that minimal repro; the build owner
  fixed CMake, and the final run uses the repaired combined-build tool.
* Canonical `Type::tuple` rejects zero/one fields; source `fn main(){let x=();}`
  is rejected. Empty/singleton tuples are explicit negative tests, **not**
  claimed native-execution support. Narrow signed types such as `i8` are also
  explicitly rejected; admitted signed integer coverage is `i64`.
* Infinite native recursion is not claimed to implement the evaluator's
  call-depth/step budgets. Finite recursion executes; evaluator depth/steps
  and native compile-time frame exhaustion are tested separately.
* Evidence is Linux AMD64 SysV only. No non-AMD64 execution, thread sanitizer,
  arbitrary aggregate/foreign ABI, or whole-language completeness is claimed.
