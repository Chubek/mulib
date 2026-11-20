# Cross-layer integration verification record

The historical sections preserve earlier evidence for MuLIB, MuLL, On1x, SDK,
and On1xRT. Fresh working-tree results are recorded separately below. Counts
describe the tested revision/configuration, not a production approval.

## Historical starting tree

Baseline: MuLIB `d1bddd5f5e4a41f8cc6231564a40d9463825bfa2`, clean;
Linux x86-64 `6.18.52-1-lts`, GCC/G++ `16.2.1`, Clang `22.1.8`,
CMake `4.4.3`, Python `3.14.7`. The in-tree LibStone checkout and gitlink
identified `3399047f6c6328b8f07788795c610699e66c060a`. A separate sibling
checkout was not substituted. At that baseline, `.gitmodules` lacked the
gitlink mapping, so `git submodule status` failed, but the local checkout
worked as `ON1X_LIBSTONE_ROOT`.

| Starting-tree check | Historical result |
| --- | --- |
| Dependency strata and manuals | Passed; manuals checked 1,960 headers. |
| MuLL inventory | Failed on missing generated registrations. |
| Default MuLIB GCC Debug build/CTest | Build passed; 744/746 tests passed. MuLL inventory and graphics metadata failed. |
| Focused MuLIB Clang ASan/UBSan aggregate/media checks | Build passed; 11/11 passed. No live codecs/devices tested. |
| On1x GCC generated scanner, ExoML extraction, Exotract Lua off | Build passed; 180 passed, one live-cgroup skip, two failures: addon manifest validation and a 180-second concurrency timeout. |

## Historical integrated results

- The addon manifest reader accepted shared family manuals after schema
  alignment. Its direct rerun passed repository smoke, shared-manual, and
  path-traversal rejection checks.
- A fallback-scanner configure with SDK/native PPX/testing disabled correctly
  recorded local revisions, worktree changes, and extraction capability.
- Root GCC Debug build and full CTest: **749/749 passed**, including refreshed
  inventories, manuals, regeneration, and C composition fixtures. Direct
  strata/manual/graphics/media/MuLL checks and compiler-backed MuLL checks passed.
- MuDSL/MuLL Debug with `MULL_ENABLE_MULIB=ON`: **6/6 selected tests passed**
  across C++/D hosts, inventory/regressions, and optimized/unoptimized
  conformance.
- Focused On1x GCC generated-scanner integration: **20/20 passed** with
  ABI-1/2 linked SDK fixtures. Full integration: **186 passed, one skipped,
  zero failed**. The skip required a delegated cgroup parent; concurrency
  and cancellation tests passed.
- Focused On1x GCC with Lua-enabled Exotract: **11/11 passed**, including raw
  wrappers, numerical composition, inventories, CDD, contracts, and existing
  kernel tests. Preliminary parallel timeouts passed serially without
  deadline changes.
- Clang fallback focused checks: **6/6 passed** with generated C11
  ASan/UBSan/LSan. The compiler/runtime CTest binaries themselves were not
  instrumented in that profile.
- Separate instrumented Clang builds passed **15/15 focused MuLIB checks**
  and **6/6 On1xRT checks**, covering lifecycle, negatives, allocation faults,
  C++/ABI, and trusted native loading.
- Strict GCC and Clang-sanitized C composition fixtures passed. The optional
  FFmpeg CLI check produced exact RGB round-trip parity; `ngspice` was absent.
  A standalone media parser gate passed a bounded **5,000-run** fuzz sample.
  Neither proves native provider linkage, codec/device support, or complete
  fuzz coverage.
- A relocated installation generated a Clang `-O2` single-member archive
  for `media-timebase:mulib_media_timebase_rescale`; its lock recorded
  separate-TU link success and denied KernelBind/safe export. The relocated
  compiler/build driver also ran a nested `Box` exchange returning `42`;
  the copy-based generic getter remained closed to move-only payloads.

The concurrency suite's outer budget was increased from 180 to 600 seconds
because it compiles many programs; per-process watchdogs were unchanged.
Cancellation timing excluded C compilation while retaining the eight-second
executable-cleanup assertion.

An overlapping Clang fallback run was not an immutable baseline. It saw
manifest/concurrency failures, cancellation timing, an intermittent Pod native
failure, and a compiler/stdlib mismatch. Rebuilt targeted profiles passed;
the overlapping run is not reported as a clean full pass.

No full sanitizer matrix, TSan, native-device execution, or non-Linux release
verification was performed for that increment. Independent semantic approval,
general foreign resource ownership, and production application integration
remain separate gates.

For current commands and limits, see [installation](../INSTALL.md),
[media review](media-review.md), [engineering status](engineering-status.md),
[On1x verification](../on1x/VERIFICATION.md), and
[representation contracts](representation-contracts.md).

## Working-tree checks — 2026-11-20

These checks used the working-tree refactor based on
`8a2c380e2866835e0812178c94b8689d317a9219`, starting from a clean tree.
An immutable starting-source snapshot passed 749 root tests and the
195-test generated-scanner profile (one delegated-cgroup skip).
The LibStone revision remained
`b5bf6d916cf09bec8734e750468c15a725e7baac`.
Host: Linux x86-64/glibc, GCC 16.2.1, Clang 22.1.8, CMake 4.4.3.
Builds and logs were kept outside the source tree.

| Fresh profile | Result |
| --- | --- |
| Root GCC Debug, strict warnings | 751 passed, zero failed |
| Root Clang Debug, `MULIB_SANITIZERS=ON` | 751 passed, zero failed |
| On1x GCC, required LibStone frontend, ExoML extraction, Exotract and Lua discovery | 194 passed, one skipped, zero failed |
| On1x Clang, fallback scanner | 178 passed, one skipped, zero failed |
| Explicit linked SDK ABI-1 compiler hook and ABI-2 MIR analyzer | 2 passed, zero failed |
| On1xRT Clang ASan/UBSan, lifecycle/negative/allocation/C++/ABI/trusted loading | 6 passed, zero failed |
| MuLL native bridge, C++/D hosts, inventory and optimized/unoptimized conformance | 6 selected tests passed, zero failed |
| Clean root installation, installed substrate example with GCC and Clang | Both compiled and executed |

Both On1x skips require an authorized delegated cgroup parent. No test was
disabled or given a larger deadline. Registration comparison retained all
749 root cases and all 195 generated-scanner cases under their functional
names; two direct substrate checks were added to the root suite.

Root reproduction:

```sh
BUILD=/absolute/path/to/external-build
cmake -S . -B "$BUILD/root-gcc" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$BUILD/root-gcc" --parallel 4
ctest --test-dir "$BUILD/root-gcc" --parallel 4 --output-on-failure

cmake -S . -B "$BUILD/root-clang" -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DMULIB_SANITIZERS=ON
cmake --build "$BUILD/root-clang" --parallel 4
ctest --test-dir "$BUILD/root-clang" --parallel 4 --output-on-failure
```

The generated-scanner On1x configuration used `ON1X_REQUIRE_LIBSTONE=ON`,
`ON1X_WITH_EXOML=ON`, `ON1X_WITH_EXOML_EXTRACT=ON`,
`ON1X_WITH_EXOTRACT=ON`, and `ON1X_WITH_EXOTRACT_LUA=ON`, with
`ON1X_LIBSTONE_ROOT` pointing at the in-tree checkout. The fallback configuration
used Clang/Clang++, `ON1X_USE_FALLBACK_SCANNER=ON`,
`ON1X_WITH_SYREN=OFF`, and `ON1X_WITH_WETSTONE=OFF`.
Both built all targets and ran their complete registered CTest suites.
Those full compiler executables were not sanitizer-instrumented; the separate
runtime profile and focused emitted-C fixtures were instrumented.
The linked SDK profile selected the checked-in `sdk/tests/linked_extension.c`
and `sdk/tests/linked_extension_v2.c` through
`ON1X_LINKED_SDK_EXTENSION` and `ON1X_LINKED_SDK_EXTENSION_V2`; its focused
tests executed both real compiler hooks.

Additional focused checks covered 300 modified headers as strict C11,
five C++17 substrate/aggregate headers, separate-TU ownership, 48 layout
assertions, 194 media headers under GCC and Clang, six media regressions,
88 engineering/substrate C regressions with C++ linkage, and 5,000 parser
fuzz iterations. Circuit/media/HTTP composition and independent forecasting,
optimization, and ephemeral foreign-review fixtures passed. FFmpeg CLI
round-trip parity passed; the optional ngspice oracle was unavailable.

Canonical inventory checks passed: 1,964 manual headers, 1,965 MuLL header
registrations, 7,283 discovered declarations, and an unchanged 5,373 raw
functions. The standard-library generator refreshed source hashes, and all
673 recorded files matched. Contract lint retained `gate_b=false` and
`safe_export=false`; refreshed unreviewed fingerprints confer no admission.

These results do not establish full compiler sanitizer/TSan coverage,
cross-platform support, live GPU/audio execution, general foreign safety, or
unrestricted aggregate-header compatibility. In particular, the existing
incompatible `mulib_dsim_resolve` declarations in separate digital-simulation
headers remain outside the supported combined profile.
