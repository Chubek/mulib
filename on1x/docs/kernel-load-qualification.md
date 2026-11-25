# Load-time kernel imports: qualification

The acceptance product was built on glibc Linux AMD64 with Clang 22.1.8 and
also exercised with GCC 16.2.1. MuLIB started on `master` at
`b4324884ab981f1ad6c1570fd735592810bec1a6`. Its LibStone gitlink is
`ac95160900398cb864a91b134fc6d716b29c0a28`; the pre-existing checkout was
`76d24a7f8b19294bab2b713dc69f799e412b62d2`. The latter was not changed.
The qBFD, qDSL and Cephyr source used here did not differ between the two
revisions. [Independent qualification](../tests/kernel_load_qualify_evidence.md)
records the pinned-vs-active baseline comparison.

## Reproduce the build and installed demo

Use a LibStone checkout with Cephyr's build dependencies populated. The
pre-existing Cephyr installation on this machine lacked `cephyr_emit_mir`;
it could not run the current FFIHandler generator. The working Cephyr here
was rebuilt in an isolated directory:

```sh
mkdir -p "$PWD/build/kernel-load-tmp"
export TMPDIR="$PWD/build/kernel-load-tmp"
libstone=/mnt/nvme/libstone
cephyr="$PWD/build/kernel-load-cephyr-prefix"
crt="$PWD/build/portable-ffi-prefix/share/cephyr/native-packages"
# The checkout's scripts/aurocks.pl symlink is dangling. Use the historical
# compatible standalone Perl driver that accepts --no-strpool.
git -C "$libstone" show 7878cb91cecf01926e8f0fe441bedd6e19ca03ac:cornerstone/Aurocks/scripts/aurocks.pl \
  > "$TMPDIR/aurocks.pl"
cmake -S "$libstone/cornerstone/Cephyr" -B build/kernel-load-cephyr \
  -DCEPHYR_BUILD_TESTS=OFF -DCEPHYR_PREBUILT_RUNTIME_ROOT="$crt" \
  -DYELLOWSTONE_LDSCRIPT_AUROCKS="$TMPDIR/aurocks.pl" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_INSTALL_PREFIX="$cephyr"
cmake --build build/kernel-load-cephyr --parallel 5
cmake --install build/kernel-load-cephyr
```

The `crt` path above is a local prerequisite used because cross GCC for
AArch64 and RV64 was unavailable. An already working Cephyr with the MIR
embedding API can be used instead. Configure the native-enabled On1x build
as shown in the [user guide](kernel-load-time.md), using `build/kernel-load-on1x`
as the build directory and `$PWD/build/kernel-load-prefix` as the prefix.
The recorded configuration uses the fallback scanner, Cephyr embedding and
trusted dynamic On1xRT enabled, and ArchVirt, Archon, native PPX, Syren,
Wetstone and project QMR disabled.

From the repository root, with `cephyr` and `TMPDIR` as above:

```sh
root=$PWD
sdk="$root/build/kernel-load-prefix"
work=$(mktemp -d /mnt/nvme/kernel-load-demo.XXXXXXXX)
(
  cd "$work"
  python3 -B "$sdk/share/on1x/examples/load-time-clamp/demo.py" \
    --prefix "$sdk" --cephyr-prefix "$cephyr" \
    --header "$root/kernels/mulib-clamp.h" --cc clang \
    --work-dir "$work/demo" --allow-demo-native
  sha256sum "$work"/demo/deploy/clamp \
    "$work"/demo/deploy/portable/stdffi.clamp.on1xkp \
    "$work"/demo/deploy/native/stdffi.clamp.on1xkp
  readelf -d "$work/demo/deploy/clamp"
)
```

Observed SHA-256 identities for the Clang run:

| Artifact | SHA-256 |
| --- | --- |
| Application, unchanged across providers | `64e8fabb7de4db747293d23f8400f7921ee65761b0619f8e2d44616998fb02e0` |
| Checked-recipe package | `58db5d5418b30f34a39fc3cb5ac816ab0c7deb04d5fce4f30f477e5cc5fc0ee8` |
| Native package | `4d738adadaec193223b2ae12341d96a874713b25b337821e1a7905981f4d9d67` |
| Embedded native image | `e4f7cc673c669aa061f278bafb4803dabf6c3e2e21b80caa5fcb8da530a94ff0` |

The trace named different providers and matched both independently hashed
packages. Both returned the same 26 results as the C oracle and rejected
reversed bounds. The installed emitter produced three typed call thunks and
called `on1x_kernel_import_init` before `on1x_environ_init` and On1x `main`.
There was no embedded clamp body. The deployable application had no SDK or
Cephyr dependency, RPATH, or RUNPATH. A separate relink using qBFD objects
compiled directly from `git archive ac9516090` also ran both packages.

## Regression and sanitizer commands

The baseline was compiled from an isolated checkout of `b4324884`, using the
same compiler configuration and active LibStone tree as the integration
build. The standalone MuLIB CDD test passed 500 seeded and 24,576 exhaustive
oracle cases. Older cached KernelRT and On1xRT products were probed separately;
the clean baseline emits its build-pinned recipe directly and does not depend
on KernelRT. The current trusted On1xRT dynamic-admission test passes.

```sh
focus='kernel_load|stdffi|runtime.trusted_dynamic|cephyr.clamp_abi|project_driver|loader_api'
ctest --test-dir build/kernel-load-baseline -R 'stdffi|trusted_dynamic|cephyr.clamp_abi|project_driver' \
  -j4 --output-on-failure
ctest --test-dir build/kernel-load-baseline -j6 --output-on-failure
ctest --test-dir build/kernel-load-on1x -R "$focus" -j6 --output-on-failure
ctest --test-dir build/kernel-load-on1x -j10 --output-on-failure
```

| Suite | Passed | Failed | Skipped |
| --- | ---: | ---: | ---: |
| Baseline focused | 7 | 0 | 0 |
| Baseline full | 237 | 2 | 2 |
| Modified focused | 13 | 0 | 0 |
| Modified full | 242 | 2 | 2 |

The same two failures reproduce before and after the change:
`on1x.stdlib_project` reports `mcp/__init__.on1x` missing from the restricted
stdlib package, and `on1x.processor.snapshots` reports a stale checked-in
On1xMETA parser. `on1x.container_pod_cgroup` and `on1x.category` skip in both
runs. Disabled optional compiler providers were not qualified for this feature.
A separate native-disabled, no-Cephyr-embedding build passed portable startup
and the negative/rollback suite.

For ASan/UBSan, configure a separate Clang debug build with
`-DCMAKE_C_FLAGS="$flags" -DCMAKE_CXX_FLAGS="$flags"`, where
`flags='-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer'`,
and enable `ON1X_WITH_KERNEL_NATIVE`. The `on1x_kernel_{eval,resolve,recipe}_tests`
targets passed (3/3). The same instrumented archive, including the native
preflight and qBFD bridge, passed the malformed-package, policy, startup-order
and multi-module rollback cases when linked into generated application C:

```sh
python3 -B on1x/tests/test_kernel_load_negative.py \
  "$PWD/build/kernel-load-on1x/on1xc" "$PWD/on1x" \
  "$PWD/build/kernel-load-sanitized/test-artifacts" clang \
  --cflag=-fsanitize=address,undefined --cflag=-fno-sanitize-recover=all \
  --cflag=-fno-omit-frame-pointer --native \
  --import-library "$PWD/build/kernel-load-sanitized/libon1x_kernel_import.a"
```

The compiler and the deliberately dependency-free native image were not
instrumented. Independent differential runs passed 579 cases per provider
using a freshly relocated install and an instrumented app and importer.

Twelve worker sessions overlapped. W01–W12 all sent HELLO and acknowledged the
shared mailbox. Eleven sent DONE. W10's session ended before its final
handoff; the coordinator completed and ran its negative suite. The original
LibStone worktree state was preserved.
