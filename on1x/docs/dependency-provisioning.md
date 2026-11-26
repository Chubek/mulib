# On1x dependency provisioning and install checks

This runbook covers the Linux x86-64 qualification of MuLIB base
`26c4fc0845be2ffd120ce0d68b4b7c5ee7c95be9` and its LibStone gitlink
`76d24a7f8b19294bab2b713dc69f799e412b62d2`. It distinguishes a build from
the source pin, a build using known historical tools, and an unqualified
configuration. Neither a successful configure nor a tool's version banner
proves provenance.

## Build host

The recorded host was EndeavourOS / Linux x86-64 7.2.6, glibc 2.44,
GCC 16.2.1, Clang/LLVM 22.1.8, GNU binutils 2.47, CMake 4.4.3
(Unix Makefiles), GNU Make 4.4.1, Perl 5.42.2, Python 3.14.7,
pkgconf 3.0.7 and libyaml 0.2.5. These are evidence for this build, not
minimum version requirements. Use separate, writable output and temp
directories on a filesystem with free blocks and inodes. A full inherited
`TMPDIR` initially made GCC fail even CMake's compiler ABI detection. Set it
before configure **and** build/test, and limit parallelism on a shared host.

From the MuLIB root, start with an existing initialized LibStone checkout.
The retired `.decomissioned/lapislazuli/.info/binutils` gitlink has no
`.gitmodules` mapping at this pin; a recursive `git submodule status` can
fail there even though this build does not consume it. Do not substitute a
newer LibStone checkout or use `get-deps.sh --update` to repair the pin.

```sh
root=$PWD
libstone="$root/on1x/third_party/libstone"
git -C "$root" ls-tree HEAD on1x/third_party/libstone
git -C "$libstone" rev-parse HEAD
GIT_OPTIONAL_LOCKS=0 git -C "$libstone" diff --exit-code HEAD -- .

B="$root/build/on1x-provisioning"
mkdir -p "$B/tmp"
export TMPDIR="$B/tmp" LC_ALL=C
```

Use a **new** `$B` for each independent configure; the failed configuration
and historical prefixes are useful evidence and need not be overwritten.
The Aurocks build below consumes the tracked Aurocks, GLR, Chomsky3, Lua,
klib and ctl snapshots. The On1x configure also draws ASDL, Firestone
regex, LLSynth (including MEMGraph and the On1xMETA snapshot generator),
and additional feature-dependent inputs from this pin. `git ls-tree
<commit> <path>` identifies these source trees; `git diff` checks tracked
working files. Record any nontracked headers, imported targets, or external
tools separately. A known commit at a cache's current source path does not
establish which sources an existing binary used.

## Native Aurocks: verified source build

On this host, GCC 16.2.1 at the default Release `-O3` hit an internal
compiler error in the `ira` RTL pass on `Backend.cpp` and `Emitter.cpp`.
All-Clang builds hit a Chomsky3 `-Werror,-Wsometimes-uninitialized` diagnostic
in `codegen_bytecode.c`. The tested workaround keeps GCC for vendor C and
uses Clang++ for C++, without changing vendor sources or disabling warnings.

```sh
cmake -S "$libstone/cornerstone/Aurocks" -B "$B/aurocks" \
  -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DLIBSTONE_BUILD_TOOLS=ON -DLIBSTONE_BUILD_TESTS=ON
cmake --build "$B/aurocks" --parallel 2 --target aurocks
aurocks="$B/aurocks/aurocks"
sha256sum "$aurocks"
cmake --build "$B/aurocks" --parallel 2 --target \
  aurocks_c_api_test aurocks_linked_test aurocks_faults_portable \
  aurocks_faults_liblr aurocks_faults_libglr aurocks_linked_generated_test
ctest --test-dir "$B/aurocks" --output-on-failure -R '^aurocks\.'

mkdir "$B/scan-one" "$B/scan-two"
for out in "$B/scan-one" "$B/scan-two"; do
  python3 -IB on1x/cmake/GenerateAurocks.py "$aurocks" on1x/grammar/On1x.g "$out"
done
diff -ru "$B/scan-one" "$B/scan-two"
```

Aurocks tree `720dacd55f8fa8334ad9b9c5534c7811fa2e03db` built successfully
with all five standalone tests passing (`c_api`, `linked`, `integration`,
`faults`, `linked_generated`). This run's tool SHA-256 was
`260a6debd4d1d2a8c0ef30292d8ffaa9959aab9e3a777ebf7c8d609c654b82da`.
Both fault banks and the scanner matched byte-for-byte across the two
output directories. `on1x_parser.c` SHA-256 was
`dd400fd8f3241fc936fdbc09354ccc29ef3543145e53d5c4109f759b3f2ba1a4`.
This checks generated files from that grammar and generator, not binary
reproducibility across rebuilds or hosts.

Use `-DON1X_AUROCKS_HOST_EXECUTABLE="$aurocks"` in the On1x configure.
LibStone generators in the same configure (e.g. bundled Novostone) use
`LIBSTONE_AUROCKS_EXECUTABLE`; both consumers must use the same host binary
when external Aurocks suppresses the in-tree CLI target. A fallback scanner
build is intentionally a different configuration. It cannot qualify
generated scanner or fault-recovery behavior, and it still needs host
Cephyr for FFI extraction.

## Build-host Cephyr and optional providers

FFIHandler always needs a **shared** build-host `libcephyr`, even when
`ON1X_WITH_CEPHYR=OFF`. Select it explicitly using
`ON1X_STDFFI_CEPHYR_LIBRARY`; native builds can alternatively select an
installed CMake package via `Cephyr_DIR`. The Python bindings and default
config for FFI extraction come from the selected LibStone source tree.
Setting `ON1X_WITH_CEPHYR=ON` is separate: it requires the installed
`Cephyr::Cephyr` target with embedding ABI 1 and the C11 syntax and declaration
interface APIs. It currently supports native AMD64 Linux LP64, not arbitrary
cross builds.

The pinned Cephyr source tree is `f8e44f0c23e8161b5cdeb69e007a3f0ab9646cc6`.
Its standalone build expects `third_party/valijson`, but LibStone's
tracked Valijson only exists under
`third_party/termlib/third_party/openaipp/third_party/EZDox/third_party/valijson`
(tree `70f3af45d7b9345b0e9ccd3d4b9112b968833790`). The standalone
configuration using a nontracked top-level copy is not an exact dependency
closure; that copy differs from the nested tracked headers. Cephyr already
finds tracked nlohmann JSON under
`third_party/libglr/third_party/libshell/third_party/taskflow/3rd-party`.
An upstream configure override for Valijson would allow selecting the
tracked tree without editing the checkout.

When target GCCs are unavailable, the target CRT and libc can be built
from the pinned source with Clang's freestanding integrated cross
assembler and `llvm-ar`. This does not require target system headers.

```sh
for row in 'AMD64 x86_64-linux-gnu' 'AArch64 aarch64-linux-gnu' \
           'RISCV64IM riscv64-linux-gnu'; do
  set -- $row
  env CC=/usr/bin/clang AR=/usr/bin/llvm-ar \
    CEPHYR_LIBC_TARGET="$2" CEPHYR_LIBC_TARGET_OPTION="--target=$2" \
    CEPHYR_LIBC_CFLAGS='-fno-pic -fno-pie' \
    sh "$libstone/cornerstone/Cephyr/stdlib/build.sh" "$B/runtime/$1"
  llvm-readelf -h "$B/runtime/$1/crt0.o"
  sha256sum "$B/runtime/$1/crt0.o" "$B/runtime/$1/libc.a"
done
```

Supply that root using `-DCEPHYR_PREBUILT_RUNTIME_ROOT="$B/runtime"` in a
standalone Cephyr build. All six outputs matched on a second build to
different directories on this host; `llvm-readelf` identified the three
CRT ELF machines and RV64 flags `0x0` (non-compressed, soft-float). This is
not evidence of guest execution or of a complete Cephyr package.

**Blockers and scoped alternatives:**

- Cephyr builds package-private `qobjld`. Its QLD expression parser depends
  on deleted `scripts/aurocks.pl`, and `YELLOWSTONE_LDSCRIPT_AUROCKS`
  expects a Perl generator. The native Aurocks CLI accepts `ldscript.g`,
  but its emitted C does not compile with the grammar's legacy
  `GLRParser`, `glr_depth`, `glr_error_pos`, and `glr_qld_expr` wrapper.
  Repointing the Perl path or swapping binaries cannot migrate that ABI.
  The legacy Perl input from LibStone
  `7878cb91cecf01926e8f0fe441bedd6e19ca03ac` can be extracted to a build
  directory without changing either checkout. When passed to
  `GenerateFrontend.cmake` with pinned ASDL, it generated QLD C that
  passed GCC C11 syntax checking. This does not qualify the entire
  standalone Cephyr build.
- Default AMD64 Archon provisioning
  (`python3 -IB on1x/backends/mir-archon/provision_toolchain.py
  --libstone-root "$libstone" --work-dir "$B/archon" --jobs 2`) compiles
  `amd64.archdb`, then fails in `irgen/translate_tool.py` trying to embed the
  also-deleted `cornerstone/Aurocks/scripts/aurocks.pl`. This is not a
  failure to find the native Aurocks executable. For the narrower native C
  backend package, explicitly disable the unrelated source-to-ANF
  `Translate.py` stage; the resulting package builds from the exact pin
  and needs no historical generator. The recipe below does not provision
  the source translator or Oracle IPC.
- The optional ExoML parser independently expects the deleted
  `scripts/aurocks.pl`; Exotract's extraction build depends on ExoML.
  An explicitly recorded source projection with the same historical
  Perl bootstrap can build that extractor, but cannot turn it into a
  pristine exact-pin build.
- Satie EUF is an explicit projection, not an unmodified full-Satie
  build. For optional `ON1X_WITH_NEROTYPICAL=ON`, run
  `python3 -IB on1x/tools/prepare-type-sdk.py --output "$B/satie-euf"`
  and set `-DON1X_SATIE_ROOT="$B/satie-euf"`. Preserve
  `on1x-sdk-provenance.json` (original file, patch, and patched CMake
  hashes); a copy made by an older helper could silently skip the patch
  if its destination was under the MuLIB worktree. Confirm it has
  `SATIE_EUF_ONLY` before using it. The projection consumes the pin's Satie
  tree `bad9731527b570b099e6f75d9d651cab4c5f4d96`; the run's patch SHA-256
  was `78d6af779f1c9b6ffdd8b6c96c1d8c95bdd8f21507dbbbe0cd1493d097fdc5d7`.

To reproduce the historical-bootstrap QLD parser check:

```sh
mkdir "$B/qld"
git -C "$libstone" show \
  7878cb91cecf01926e8f0fe441bedd6e19ca03ac:cornerstone/Aurocks/scripts/aurocks.pl \
  >"$B/qld/aurocks.pl"
sha256sum "$B/qld/aurocks.pl"
# Expect 9f192e8175dcdb54993fff8c608808c0ab2866f39e644f4008528b61f8150a78.
qld="$libstone/yellowstone/ld"
cmake -DPERL=/usr/bin/perl "-DASDL=$libstone/scripts/asdl.pl" \
  "-DAUROCKS=$B/qld/aurocks.pl" "-DSOURCE=$qld" "-DOUT=$B/qld" \
  -P "$qld/GenerateFrontend.cmake"
gcc -std=c11 -I "$B/qld" -I "$qld" -fsyntax-only "$B/qld/ldscript_expression.c"
```

The corresponding standalone Cephyr cache option is
`-DYELLOWSTONE_LDSCRIPT_AUROCKS="$B/qld/aurocks.pl"`; the overridden
input must appear in provenance. The configure below also maps the
expected Valijson path to its tracked source in a build-owned projection.
The projection uses symlinks and therefore needs the checked-out input
tree to stay available and unchanged.

```sh
stage="$B/cephyr-source-layout"
mkdir -p "$stage/third_party"
git -C "$libstone" ls-tree --name-only HEAD |
  while IFS= read -r entry; do
    if test "$entry" != third_party && test -e "$libstone/$entry"; then
      ln -s "$libstone/$entry" "$stage/$entry"
    fi
  done
git -C "$libstone" ls-tree --name-only HEAD:third_party |
  while IFS= read -r entry; do
    ln -s "$libstone/third_party/$entry" "$stage/third_party/$entry"
  done
ln -s 'termlib/third_party/openaipp/third_party/EZDox/third_party/valijson' \
  "$stage/third_party/valijson"
cmake -S "$stage/cornerstone/Cephyr" -B "$B/cephyr-build" \
  -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Release \
  -DCEPHYR_BUILD_TESTS=OFF -DCEPHYR_ENABLE_MOONSTONE=OFF \
  "-DCEPHYR_PREBUILT_RUNTIME_ROOT=$B/runtime" \
  "-DYELLOWSTONE_LDSCRIPT_AUROCKS=$B/qld/aurocks.pl"
cmake --build "$B/cephyr-build" --parallel 2
cmake --install "$B/cephyr-build" --prefix "$B/cephyr-projection-prefix"
```

With GCC 16.2.1 this built and installed on retry after an internal
compiler error in the first attempt. The resulting shared library
passed the independent AMD64 C object and FFISpec fixtures, and the
full header scan. See [release evidence](release-qualification.md#results-and-limits).
It still uses the explicit historical QLD bootstrap and doesn't qualify
Cephyr's other execution targets or replace the earlier On1x suite's
host package provenance.

The following Archon recipe does not use that historical file:

```sh
(
  cd "$libstone/cornerstone/Archon"
  python3 -B -m archon compile stdspec/amd64/root.arch -o "$B/amd64.archdb"
  python3 -B -m archon irgen gated --archdb "$B/amd64.archdb" \
    --profile native --abi SysVAMD64 --no-oracle \
    --option translate_tool=false -o "$B/archon-generated"
)
cmake -S "$B/archon-generated" -B "$B/archon-build" -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$B/archon-prefix"
cmake --build "$B/archon-build" --parallel 2
cmake --install "$B/archon-build"
```

Archon tree `480f09cabea6fce5a2276b98b5e068f1284dcaa9` generated and
installed `Archon::Backend`; the static archive SHA-256 was
`bfe88ccfe1d139cada2ac1842a382b3c98038e5b4fd8a4775b2c468dafb548b1`.
The installed tool accepted a constant-return AMD64 gated MIR fixture,
emitted GAS, and its separately C-linked executable returned `42` to
the harness. This is standalone provider evidence, not evidence for the
On1x adapter. Select this package with
`-DArchonBackend_DIR="$B/archon-prefix/lib/cmake/ArchonBackend"`.

A provisional configuration can use an explicitly selected historical
Cephyr (or previously built Archon) package to continue feature testing.
Record its path, file hash, and source/configure evidence if available,
and distinguish every test that depends on it. Do not report a clean
source-only release build on the strength of those results. Pinning and
porting the missing generator interfaces belongs upstream in LibStone.

## Independently move and consume an installation

After a completed On1x build, run the install check from the MuLIB root.
Allow `cmake --install` to write its install manifest only to the selected
build directory; do not run concurrent install tests on that same build.
Use a new work path outside the checkout on a writable filesystem:

```sh
python3 -IB on1x/tools/qualify-installed-sdk.py \
  --build "$B/on1x" --work /path/to/new-sdk-check --require-qmr
```

For the independently tested fallback profile, configure the install
input as follows, with `cephyr_dir` naming a separately identified
historical host Cephyr CMake package:

```sh
cmake -S "$root/on1x" -B "$B/on1x-fallback" -G 'Unix Makefiles' \
  -DCMAKE_C_COMPILER=/usr/bin/cc -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DON1X_REQUIRE_LIBSTONE=OFF -DON1X_USE_FALLBACK_SCANNER=ON \
  -DON1X_WITH_CEPHYR=OFF -DON1X_WITH_PROJECT_QMR=ON \
  "-DON1X_LIBSTONE_ROOT=$libstone" "-DCephyr_DIR=$cephyr_dir"
cmake --build "$B/on1x-fallback" --parallel 2
python3 -IB on1x/tools/qualify-installed-sdk.py \
  --build "$B/on1x-fallback" --work /path/to/new-fallback-sdk-check --require-qmr
```

The host Cephyr binary used by that run had SHA-256
`b01c7b106e10944938ef0a90ba44676fa296b7dfe7312cf6e9ec7bef1dcbb987`;
it was not built by the fallback recipe. The relocated `on1xc` SHA-256
was `a28a712960a07e98fee6acc8ee5801342fb3ee236ed1bcc2be795e332283d7bd`.
The SDK and project checks passed on that fallback build, including QMR.
This does not qualify generated scanner or optional backend installation.

Omit `--require-qmr` for builds intentionally configured without QMR.
The checker moves the complete install to a prefix containing a space,
configures an independent C++17 `On1x::Project` consumer, runs the
installed Python binding, validates a manifest and exact v4 lock with
the installed CLI, checks that a missing validator fails closed, and
builds and runs an offline project with its installed Mune helper.
If installed, it also exercises QMR-to-Mune rendering. It retains per-step
stdout/stderr and the resolved project build record. `--copy-installed`
exercises a historical installation without writing to it, but does
**not** substitute for a fresh install test.

This verifies the SDK, compiler project dispatch, and installed project
helpers. It does not claim that optional interpreter, C11 and CDecl
provider prefixes, kernel-loading tools, executable backends, or dynamic
plugins can be moved together or independently without their separately
qualified external runtime dependencies.
