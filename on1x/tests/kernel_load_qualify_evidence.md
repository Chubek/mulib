# Independent clamp qualification (W12)

Linux AMD64; GCC 16.2.1, Clang 22.1.8. Artifacts and logs are in
`/mnt/nvme/kernel-load-W12.RdUshEgw` (`W` below), outside the checkout.
`/mnt/andromeda/tempdir` was full; all host builds used `TMPDIR=$W`.

## Clean baseline and dependency pin

The MuLIB tree was archived from `b4324884ab981f1ad6c1570fd735592810bec1a6`.
Its LibStone gitlink is `ac95160900398cb864a91b134fc6d716b29c0a28`; the
working checkout was `76d24a7f8b19294bab2b713dc69f799e412b62d2`. The pin
is an ancestor, and the two trees differ in Aurocks and LLSynth, among other
components. No differences were observed under Cephyr, Archon or
`yellowstone/qbfd`. W12 built the pin side independently from a detached sparse
checkout. The comparison used COORD's clean `build/kernel-load-baseline`
compiler on the `76d24a7` side and the same fresh Cephyr library on both sides.

```sh
export W=/mnt/nvme/kernel-load-W12.RdUshEgw TMPDIR=/mnt/nvme/kernel-load-W12.RdUshEgw
cmake -S "$W/mulib-baseline/on1x" -B "$W/baseline-pin" \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS_DEBUG=-O0 -DCMAKE_C_FLAGS_DEBUG=-O0 \
  -DBUILD_TESTING=ON -DON1X_REQUIRE_LIBSTONE=ON -DON1X_LIBSTONE_ROOT="$W/libstone-pin" \
  -DON1X_WITH_CEPHYR=ON -DCephyr_DIR="$PWD/build/kernel-load-cephyr-prefix/lib/cmake/Cephyr" \
  -DON1X_CEPHYR_RESOURCE_ROOT="$PWD/build/kernel-load-cephyr-prefix/share/cephyr" \
  -DON1X_WITH_PROJECT_QMR=OFF -DON1X_WITH_SYREN=OFF
cmake --build "$W/baseline-pin" --target on1xc -j2
python3 -B on1x/tests/kernel_load_qualify.py "$W/oracle-clean-pin" \
  --baseline "$W/baseline-pin/on1xc" --baseline-guard unchecked
python3 -B on1x/tests/kernel_load_qualify.py "$W/oracle-clean-76d-observed" \
  --baseline build/kernel-load-baseline/on1xc --baseline-guard unchecked
```

Both clean baselines matched all 579 legal inputs under `--run-ir` and
`--run-vm`. Both left the reversed-bound case unchecked (exit 0, trace
`1,3,2,3,17001`). The generated `clamp.ffi` on both sides had `(guard true)`
on `i64` and SHA-256 `4e40390f6ed5edad450ef673d3c5296091574c28d7e8fca8b1b0cf08be1b0787`.
For `portable_clamp.on1x`, `--emit-c` and `--emit-cephyr-object` each produced
byte-identical artifacts across the two clean builds; host-linking each with
`on1x_runtime.c` printed `10` then `9223372036854775807`.

The older, pre-existing `build/on1x-portable-ffi/on1xc` is not equivalent to
that baseline: it rejected the reversed bounds in MIR/VM and its direct C
used `on1x_kernel_rt_builtin_i64`. Its direct C linked and ran using
`libon1x_kernel_rt.so`; its default `--emit-cephyr-object` failed with
`On1xKernelRT.h` not found. This stale artifact is not used for the clean
comparison.

## Installed product and sanitizers

The two package files were built from `kernels/mulib-clamp.h` using
`on1x/tools/on1x-kernel-package.py build`, the fresh Cephyr prefix, and the
regenerated `$W/baseline-pin/hir/FFIHandler/generated/FFIHandler.py`.
The host policy was written separately using the test image digest and the
driver's independent expected interface and behavior digests.

```sh
recipe="$W/qualify-pkg-portable/stdffi.clamp.on1xkp"
native="$W/qualify-pkg-native/stdffi.clamp.on1xkp"
policy="$W/trusted-provider.policy"
python3 -B on1x/tests/kernel_load_qualify.py "$W/oracle-installed-final" \
  --install-build build/kernel-load-on1x --baseline "$W/baseline-pin/on1xc" \
  --baseline-guard unchecked --recipe "$recipe" --native "$native" --policy "$policy"
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  python3 -B on1x/tests/kernel_load_qualify.py "$W/oracle-san-product-final" \
  --compiler "$W/sanitize-76d/on1xc" --builder on1x/tools/on1x-build.py --cc clang \
  --cflag=-fsanitize=address,undefined --cflag=-fno-sanitize-recover=all \
  --cflag=-fno-omit-frame-pointer --recipe "$recipe" --native "$native" --policy "$policy"
```

Both runs passed. The fresh install was relocated before generating or
linking the application. The same linked application matched all 579 values
under each provider and reported each exact package identity. Both providers
rejected the invalid interval with exit 70 after printing `1,3,2` in order.
Unset, empty and package-free roots, and a native package without explicit
host policy, failed before entering user code. `readelf -d` on the installed
app reported only `libstdc++.so.6`, `libgcc_s.so.1` and `libc.so.6` as
`NEEDED`.

For the sanitizer run, W12 built the importer archive from an external source
snapshot and clean actual-`76d24a7` LibStone using Clang ASan/UBSan, then
copied the integrated compiler beside that archive and its link sidecar.
The app and importer were instrumented; the compiler and restricted native
image were not. There were no sanitizer reports. Logs:
`installed-qualification-final.log`, `sanitizer-qualification-final.log`,
`build-pin.log` and `build-sanitize.log` under `$W`.
