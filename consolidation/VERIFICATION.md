# Consolidation verification

## Starting tree

MuLIB `d1bddd5f5e4a41f8cc6231564a40d9463825bfa2` was clean. Linux x86-64
(`6.18.52-1-lts`), GCC/G++ `16.2.1`, Clang `22.1.8`, CMake `4.4.3`, Python
`3.14.7`. The in-tree LibStone checkout and tracked gitlink both identify
`3399047f6c6328b8f07788795c610699e66c060a`. The separate sibling LibStone
checkout is a different revision and is not substituted. On the starting
tree, `git submodule status` failed because `.gitmodules` lacked the mapping.
The checkout itself was usable as `ON1X_LIBSTONE_ROOT`.

The following ran on the starting tree:

| Check | Result |
|---|---|
| `python3 tools/stratify.py --check` | Passed; dependency strata current. |
| `python3 tools/manuals.py --check` | Passed; 1,960 headers. |
| `python3 mudsl/scripts/check_mulib_inventory.py --include include` | Failed; checked-in MuLL inventory lacks registrations. |
| Default MuLIB GCC Debug build and CTest | Build passed. 744/746 passed; `mulib_mull_inventory` and `mulib_graphics_gate` failed on stale generated metadata. |
| MuLIB Clang Debug ASan/UBSan, aggregate header and JoltFX checks | Build passed; 11/11 passed in the focused subset. No live codec/device test in this profile. |
| On1x GCC Debug, generated scanner, ExoML extraction and Exotract CLI (Lua off) | Build passed. 180 passed, 1 skipped (`on1x.container_pod_cgroup`), 2 failed (`on1x.addon_chatgpt`, `on1x.concurrency` timed out at 180 seconds). |

Reproduction (use a new external `BUILD` directory):

```sh
cmake -S . -B "$BUILD/root" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$BUILD/root" --parallel 5
ctest --test-dir "$BUILD/root" --parallel 4 --output-on-failure

cmake -S on1x -B "$BUILD/on1x" -DCMAKE_BUILD_TYPE=Debug \
  -DON1X_REQUIRE_LIBSTONE=ON -DON1X_WITH_EXOTRACT=ON \
  -DON1X_WITH_EXOML=ON -DON1X_WITH_EXOML_EXTRACT=ON
cmake --build "$BUILD/on1x" --parallel 4
ctest --test-dir "$BUILD/on1x" --parallel 4 --output-on-failure
```

The initial Ninja configure did not run a build (Ninja is not installed);
the Make-based runs above replace it. Exotract requires both ExoML flags.

The focused Clang profile used `-DCMAKE_C_COMPILER=clang
-DCMAKE_CXX_COMPILER=clang++ -DMULIB_SANITIZERS=ON -DMULIB_BUILD_MUW3=OFF
-DMULIB_BUILD_SHIRAZ=OFF -DMULIB_BUILD_VIRTIO=OFF`. Built `mulib_all_headers`,
`mulib_joltfx_pipeline`, `mulib_joltfx_link`, and the six JoltFX regression
executables. Ran CTest with `-R '^mulib_(all_headers$|joltfx[-_])'`.

## Principal integration checks

- `on1x.addon_chatgpt` failed at baseline because its manifest reader rejected
  implementation-switch modules using the existing shared Amb3r manual.
  The direct CTest rerun after the schema-alignment fix passed, including the
  real repository smoke, shared-manual fixture and path-traversal rejections.
- Configured a fallback scanner with SDK/native PPX/testing off and checked
  `on1x-build-info.txt` against both local Git revisions. It reported the
  tracked worktree changes and the disabled bundled extraction profile.
- Default root GCC Debug build and full CTest: **749/749 passed**, including
  the two formerly stale inventories, manual and regeneration checks, and
  the C application workloads. Direct checks for strata, manuals, graphics,
  JoltFX, and MuLL passed. Compiler-backed MuLL `--check` also passed.
- MuDSL/MuLL Debug with `-DMULL_ENABLE_MULIB=ON`, built targets `mull_tests`
  and `mull_d_test_build`: **6/6 passed** with
  `-R '^mull_(host|host_d|mulib_inventory|mulib_inventory_regressions|conformance|conformance_unoptimized)$'`.
- On1x focused GCC generated-scanner integration: **20/20 passed**.
  Configured the baseline build with `ON1X_LINKED_SDK_EXTENSION` and
  `ON1X_LINKED_SDK_EXTENSION_V2` pointing to the respective
  `on1x/sdk/tests/linked_extension{,_v2}.c` sources. Tested MIR and passes,
  ownership, SDK analyzer and ABI-1/2 dispatch, runtime lifecycle and trusted
  loader, addon, and the installed SDK consumer.
- Full integrated On1x GCC Debug/generated-scanner CTest with those linked
  fixtures and native Exotract (Lua off): **186 passed, 1 skipped, 0 failed**.
  The skip was the live Pod cgroup test, which needs a delegated parent.
  `on1x.concurrency` and `on1x.cancellation_deadlines` both passed.
- Focused integrated On1x GCC with Lua-enabled Exotract: **11/11 passed**.
  It includes the raw wrappers, the On1x Shylock workload, two inventories,
  CDD discovery, contract checks, and existing kernel build/review/IPC tests.
  A preliminary parallel run had timeouts in CDD discovery, sandbox archive,
  and imports; serial execution passed without changing their deadlines.
- On1x Clang fallback focused **6/6 passed** with generated C11
  ASan/UBSan/LSan, including MIR parity, ownership, ABI-2-disabled rejection
  and the ordinary trusted-loader fixture. The flags were
  `-DON1X_TEST_C_FLAGS=-fsanitize=address,undefined;-fno-sanitize-recover=all;-fno-omit-frame-pointer`
  (pass as one shell argument), `ASAN_OPTIONS=detect_leaks=1`,
  `-DON1X_USE_FALLBACK_SCANNER=ON` and `-DON1X_BUILD_NATIVE_PPX=OFF`.
  The compiler and runtime CTest binaries were not instrumented in that
  particular profile.
- Separate Clang ASan/UBSan/LSan builds exercised instrumented native code:
  **15/15** MuLIB focused checks (aggregate C, JoltFX, CSR, Amb3r,
  separate-TU C/C++, and workload checks) and **6/6** On1xRT checks
  (lifecycle, negatives, allocation faults, C++/ABI, and trusted native
  loading). The runtime build applied the sanitizer flags to
  `CMAKE_C_FLAGS` and `CMAKE_CXX_FLAGS`.
- The integrated C workload checks passed with strict GCC and with strict
  Clang ASan/UBSan/LSan. FFmpeg produced exact RGB round-trip parity;
  `ngspice` was unavailable. See [the reproduction guide](workloads/README.md).
- `python3 tools/check_joltfx.py --fuzz-runs 5000 --report "$BUILD/joltfx.json"`
  passed standalone header checks and a bounded 5,000-run parser fuzz sample
  after integration. It does not test native codecs or devices.
- Installed On1x into a temporary prefix, moved the prefix, and invoked
  the installed `share/on1x/tools/on1x-exotract.py --root "$PWD" wrap-inline`
  with `media-timebase:mulib_media_timebase_rescale` and the Lua-enabled
  Exotract binary. Clang `-O2` built a single-member `libmulib.a`; the lock
  reported separate-TU link success and denied KernelBind/safe export.
  The relocated compiler and build driver also compiled and ran a nested
  `Box` exchange; consuming the outer box with `take` and an owning record
  match returned `42`. The copy-based generic getter remains closed to
  move-only payloads.

To reproduce the Lua-enabled selection, configure the On1x build above
with `-DON1X_WITH_EXOTRACT_LUA=ON -DON1X_BUILD_NATIVE_PPX=OFF`, then:

```sh
cmake --build "$BUILD/on1x-lua" --parallel 2 --target on1xc exotract on1x-bind
ctest --test-dir "$BUILD/on1x-lua" --parallel 1 --output-on-failure \
  -R '^on1x\.(inline_wrappers_sdk|workload\.shylock|kernel_(history|review|ipc|imports|sandbox)_sdk|contracts_sdk|bounded_kernel_(inventory|discovery_sdk)|cdd_discovery_sdk)$'
```

The baseline concurrency suite hit its cumulative 180-second budget both
under the full suite and on a direct rerun. The suite compiles many separate
programs; its outer budget is now 600 seconds while its per-process watchdog
is unchanged. A separate cancellation test had counted C build time against
the eight-second child cleanup assertion. It now times only the executable
and retains the same assertion.

An initial full Clang fallback run overlapped source integration and is not
an immutable baseline. It saw the manifest error and concurrency timeout,
one cancellation timing failure, one intermittent Pod native failure (the
standalone Clang rerun passed), and one mismatched compiler/stdlib snapshot
for `ref_replace`. The rebuilt Clang profile passed the targeted
compiler/stdlib and managed-lifecycle checks above.

No full sanitizer matrix, TSan, native device execution, or non-Linux
release verification was run for this increment.

Local JoltFX `30b787b` and Shylock `f3e2f3a` were inspected as read-only
reference checkouts. JoltFX's `.jfx` fixture, execution SDK and Vulkan
sources confirm separate JBC1 v1 stack and v2 typed CPU profiles; this
consolidation does not replace them with MuLIB scalar IR.
