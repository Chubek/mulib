# Reproducing the native toolchain

Run from the MuLIB checkout:

```sh
python3 on1x/backends/mir-archon/provision_toolchain.py \
  --work-dir /mnt/andromeda/tempdir/opencode/on1x-archon-toolchain --jobs 4
cmake -S on1x -B /mnt/andromeda/tempdir/opencode/on1x-archon-build \
  -DCMAKE_PREFIX_PATH=/mnt/andromeda/tempdir/opencode/on1x-archon-toolchain/install \
  -DON1X_BUILD_MIR_ARCHON=ON -DON1X_ARCHON_ENABLE_SDK_ADAPTER=ON \
  -DON1X_ARCHON_TARGET_TRIPLE=x86_64-unknown-linux-gnu \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build /mnt/andromeda/tempdir/opencode/on1x-archon-build --parallel 4
ctest --test-dir /mnt/andromeda/tempdir/opencode/on1x-archon-build \
  -R 'on1x.backends|on1x.mir|on1x.sdk.backend' --output-on-failure
```

`--work-dir` can be any writable directory outside the source tree.
`--libstone-root` selects a different, already available LibStone checkout.
Provisioning does not download dependencies or change checkout revisions.
It creates the canonical ArchDB, generates the native profile, builds the
generated C99 library and CLI, runs the upstream scalar GAS suite, and installs
the actual `ArchonBackend` CMake package. A failed build or test stops
installation. `on1x-toolchain.json` records the source revision and paths.

At LibStone `76d24a7f8b19294bab2b713dc69f799e412b62d2`, full generation
with `Translate.py` requires a removed Perl generator. Use the
[backend-only provisioning recipe](../../docs/dependency-provisioning.md#build-host-cephyr-and-optional-providers)
for the independently tested native package. On1x stages that package's
`BackendManifest.json` in the build and install trees for project builds.
`ON1X_ARCHON_BACKEND_MANIFEST` can select an absolute manifest path for a
nonstandard package layout.

The default CMake build includes Yellowstone's real registrar generator.
`-DON1X_USE_YELLOWSTONE_LINK=OFF` selects the checked-in portable registrar map.
Both guard the native registrar on the actual SDK adapter target. Disabling
`ON1X_ARCHON_ENABLE_SDK_ADAPTER` still builds the standalone MIR tool and ABI
archive without registering a native SDK provider.

For a standalone build with the entire extension SDK disabled:

```sh
cmake -S on1x -B /mnt/andromeda/tempdir/opencode/on1x-archon-standalone-build \
  -DCMAKE_PREFIX_PATH=/mnt/andromeda/tempdir/opencode/on1x-archon-toolchain/install \
  -DCMAKE_INSTALL_PREFIX=/mnt/andromeda/tempdir/opencode/on1x-archon-standalone-install \
  -DON1X_BUILD_EXTENSION_SDK=OFF -DON1X_BUILD_MIR_ARCHON=ON \
  -DON1X_ARCHON_TARGET_TRIPLE=x86_64-unknown-linux-gnu \
  -DON1X_ARCHON_ENABLE_SDK_ADAPTER=OFF -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build /mnt/andromeda/tempdir/opencode/on1x-archon-standalone-build --parallel 4
ctest --test-dir /mnt/andromeda/tempdir/opencode/on1x-archon-standalone-build \
  -R 'on1x.backends.mir_archon|on1x.mir' --output-on-failure
cmake --install /mnt/andromeda/tempdir/opencode/on1x-archon-standalone-build
```

This installs `bin/on1x-archon-mir`, `lib/libon1x_archon_scalar_abi.a`,
`lib/libon1x_runtime.a` and the selected backend manifest alongside the
normal On1x compiler/runtime distribution. Enabling the SDK adapter while
disabling the SDK is rejected at configuration time.

## Initial real-toolchain evidence

On Linux AMD64, with GNU C/C++ 16.2.1:

- MuLIB: `94b27511328c61a219c90cf531ca68e952bc2a85`.
- Bundled LibStone: `5d733d69f19fac1d8a05294fe54e4a4ec863fc4c`.
- Installed manifest provider: `amd64-sysv-elf-lp64`, ABI `SysVAMD64`.
- Canonical ArchDB fingerprint:
  `530c92fee80327e1c9a4923144e4ee3996fb6b7226319294f74c9d6ce86f2ede`.
- Upstream `test_native_scalar_gas.py`: 16 tests, successful, with nine
  AArch64 test/subtest skips requiring unavailable `qemu-aarch64-static`.
  AMD64 arithmetic, CFG, memory, checked traps, floating-point and mixed ABI,
  PIC/interposition, variadic imports, cancellation, output limits, and
  canonical fingerprint rejection ran successfully.
- Real installed-header On1x bridge and tuple-phi tests passed.
- Bridge C++20 and ScalarABI C11 syntax checks passed with
  `-Wall -Wextra -Wpedantic -Werror`.
- Full combined Debug build succeeded. `on1x/examples/mir.on1x` was emitted
  as verified MIR by the combined `on1xc`, lowered to assembly by
  `on1x-archon-mir`, assembled and linked with `libon1x_archon_scalar_abi.a`,
  `libon1x_runtime.a`, and `-lm`. Its exit status (0), stdout
  (`1\n1\n2\n6\n24\n120\nfalse\n`), and stderr matched the separately
  compiled `--emit-ir-c` reference.
- Targeted On1x backend, MIR, and SDK backend tests passed (14 tests, including
  a rerun after correction of a transient bridge-test tuple fixture).
- SDK-disabled standalone build/install and all five selected MIR/backend
  tests passed. The installed compiler, native tool, ABI archive and runtime
  sources produced the same native example result. Neither installed tool
  requires SDK/Archon shared libraries. A mismatched requested target was
  rejected without publishing assembly.
- The additional `on1x.backends.mir_archon_tool_target` regression passes,
  checking that CMake supplies the actual standalone tool target definition.

These are scoped toolchain checks. See the
[release checklist](../../docs/release-qualification.md) for current language,
target, and release gaps.
