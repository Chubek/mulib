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
