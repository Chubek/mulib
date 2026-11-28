# Kernel-free native publication hardening

Base: `58f109f8`; no deleted stdlib files restored. Ordinary direct-C11
linkage no longer executes `stdlib/mulib/housekeeping.py` merely to discover
an unused include directory. An explicit `--mulib-root` (also supplied by
the project driver) still selects its kernels include directory without
importing a manifest planner. Actual source-kernel modes remain gated on
the compiler-owned helper, with an explicit diagnostic if it is unavailable.
`--stdlib` never authorizes importing Python from a source-library directory.
PPX timing runtime path lookup is independent of kernel housekeeping.

Executed with Python 3.14.7 and GCC 16.2.1:

```
TMPDIR=/mnt/warble/tempdir/opencode python3 -B on1x/tests/test_plain_native_link.py
```

Two tests passed (0.412 seconds). A mock supplies frontend C only; the real
strict C11 compiler, compiler runtime, linker and resulting executable run.
The program prints `42`. A malicious helper under the selected source library
does not run. An invalid C flag and an unavailable kernel profile preserve
the previously published executable.

The existing acceptance cases `Modules.test_manifest_fragments_graph_and_atomic_publish`
and `Modules.test_manifest_signature_seals_the_actual_build` both passed
(1.655 seconds) with:

```
TMPDIR=/mnt/warble/tempdir/opencode python3 -B on1x/tests/test_module_acceptance.py \
  --on1xc /mnt/warble/tempdir/opencode/on1x-module-semantics-XCNFi9ny-W4-build/driver/on1xc \
  --stdlib /mnt/nvme/fpnext.mI6yYw/mulib/on1x/stdlib \
  --work-dir /mnt/warble/tempdir/opencode/on1x-hardening-semantics-build/native-acceptance \
  Modules.test_manifest_fragments_graph_and_atomic_publish \
  Modules.test_manifest_signature_seals_the_actual_build
```

These use the prior integrated frontend and the modified project/build tools.
They prove fragment/signature project executables publish and run, and that
denied visibility, a wrong module name and a stale signature preserve prior
artifacts. They do not qualify source-kernel linkage, load-time kernel
packages, or separate compilation.

## Clean worker build and complete self-contained acceptance

The subsequent clean worker build uses only explicitly selected external
assets; LibStone `4607ce58`, external MuLIB `cbb7d66f`, stdlib tree `bb7e7814`.

```
BUILD=/mnt/warble/tempdir/opencode/on1x-hardening-semantics-build
cmake -S on1x -B "$BUILD" \
  -DON1X_MODULE_STDLIB_ROOT=/mnt/nvme/fpnext.mI6yYw/mulib/on1x/stdlib \
  -DON1X_LIBSTONE_ROOT=/mnt/nvme/libstone \
  -DON1X_STDFFI_CEPHYR_LIBRARY=/mnt/nvme/mulib/build/on1x-mod-lanes/cephyr-pin/libcephyr.so \
  -DON1X_WITH_WETSTONE=OFF -DON1X_WITH_SYREN=OFF \
  -DON1X_WITH_PROJECT_QMR=OFF -DON1X_WITH_KERNEL_NATIVE=OFF \
  -DON1X_BUILD_DYNAMIC_SDK_PLUGINS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" -j3
ctest --test-dir "$BUILD" --output-on-failure
TMPDIR=/mnt/warble/tempdir/opencode python3 -B on1x/tests/test_module_acceptance.py \
  --on1xc "$BUILD/on1xc" --stdlib /mnt/nvme/fpnext.mI6yYw/mulib/on1x/stdlib \
  --work-dir "$BUILD/native-acceptance-current" \
  Modules.test_dotted_receivers_do_not_bypass_named_module_scope \
  Modules.test_explicit_program_disables_transitive_source_ppx \
  Modules.test_explicit_program_main_is_not_exported_or_run \
  Modules.test_manifest_fragments_graph_and_atomic_publish \
  Modules.test_manifest_program_requires_explicit_admission \
  Modules.test_manifest_signature_seals_the_actual_build \
  Modules.test_opaque_provider_identity_and_project_atomicity \
  Modules.test_plan_projects_compiler_edges \
  Modules.test_side_by_side_resolution_and_selector_alias \
  Modules.test_source_kind_and_declared_name_errors \
  Modules.test_unit_v1_ppx_cannot_transform_program_import_graph
```

Full build completed; all 14 registered CTests passed (10.00 seconds).
All 11 self-contained acceptance cases passed (7.085 seconds). The only
unselected case requires the deleted `examples/energy-budget` fixture, which
was not restored. The earlier broad run with the prior W4 target-only build
had a single preflight failure because its adjacent `on1x-vm` was missing;
the complete worker build supplies that runner and passes the intended
opaque MIR/VM rejection tests. The first background build was stopped by a
120-second harness timeout; continuing the same build with a one-hour limit
completed successfully.
