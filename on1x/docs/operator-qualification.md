# Intrinsic operator qualification

The scanner implementations must agree on tokens and diagnostics. The source
frontend must agree with ordinary callable semantics. Passing one of those
gates does not establish the other, or that every backend admits every
ordinary callable profile.

## Reproduce with Clang

Run from the MuLIB root. `CEPHYR` is the absolute path to a build-host shared
Cephyr library for FFIHandler, as described in
[dependency provisioning](dependency-provisioning.md). The FFI generator needs
it even for the fallback scanner. `ARCHON` is the optional CMake package
directory of a **Clang-built** AMD64 Archon backend, built with the source
translator disabled as described in the same guide.

```sh
export CC=clang CXX=clang++
B="$PWD/build/on1x-operators"
G="$B/principal-generated-make"
F="$B/principal-fallback-make"

cmake -S on1x -B "$G" -G 'Unix Makefiles' \
  -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  '-DCMAKE_C_FLAGS_RELEASE=-O1 -DNDEBUG -Wno-error=sometimes-uninitialized -Wno-error=gnu-zero-variadic-macro-arguments -Wno-error=unused-function' \
  '-DCMAKE_CXX_FLAGS_RELEASE=-O1 -DNDEBUG' \
  "-DON1X_STDFFI_CEPHYR_LIBRARY=$CEPHYR" -DON1X_REQUIRE_LIBSTONE=ON \
  -DON1X_BUILD_MIR_ARCHON=ON -DON1X_ARCHON_ENABLE_SDK_ADAPTER=ON \
  -DON1X_ARCHON_TARGET_TRIPLE=x86_64-unknown-linux-gnu "-DArchonBackend_DIR=$ARCHON"

cmake -S on1x -B "$F" -G 'Unix Makefiles' \
  -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  '-DCMAKE_C_FLAGS_RELEASE=-O1 -DNDEBUG' '-DCMAKE_CXX_FLAGS_RELEASE=-O1 -DNDEBUG' \
  "-DON1X_STDFFI_CEPHYR_LIBRARY=$CEPHYR" -DON1X_USE_FALLBACK_SCANNER=ON

for dir in "$G" "$F"; do
  cmake --build "$dir" --parallel 8 || exit
  ctest --test-dir "$dir" -R 'on1x\.(operators|scanner_parity)' \
    --no-tests=error --output-on-failure || exit
done
```

Without a configured Archon package, omit the four `ON1X_BUILD_MIR_ARCHON`,
`ON1X_ARCHON_ENABLE_SDK_ADAPTER`, `ON1X_ARCHON_TARGET_TRIPLE`, and
`ArchonBackend_DIR` options. This omits the Archon execution assertion, not
the C11, MIR evaluator or VMWeave assertions. The generated profile requires
the real Aurocks scanner; it cannot silently select the fallback.

For a broader Clang-only run, use the following selection. The separate
toolchain-probe test explicitly exercises GCC discovery and compilation and
is excluded from this command. The local aliases also route any test using
plain `cc` or `c++` to Clang.

```sh
mkdir -p "$B/clang-path"
ln -s "$(command -v "$CC")" "$B/clang-path/cc"
ln -s "$(command -v "$CXX")" "$B/clang-path/c++"
export PATH="$B/clang-path:$PATH"
for dir in "$G" "$F"; do
  ctest --test-dir "$dir" -E '^on1x\.toolchain_probe$' \
    --output-on-failure --parallel 8
done
```

The isolated install check (`on1x.operators.installed`) moves the install to a
different prefix, clears On1x/MuLIB search-path overrides, and verifies that
the compiler resolves the installed `std.operators`. It executes Clang-linked
C11, reference MIR evaluation and VMWeave both from source and from saved MIR,
and optionally links and executes emitted Archon AMD64 assembly. The check
uses a scalar operator for the latter routes; the higher-order prelude
pipeline is independently checked through C11.

The generated run uses a Clang-built AMD64 backend (`SysVAMD64`, source
translator disabled). The qualification host was Clang/Clang++ 22.1.8 and
CMake 4.4.3 with LibStone `70484d40da6b546d7159ee1a9a464bc2b63954a0`.
The separately built Cephyr host shared library uses supplemental Valijson
and the historical QLD parser bootstrap described in the dependency
guide. Its SHA-256 was
`ee1201ba12e0de47cf4bcd2912c26368bf3f0c6114c6b8880cc6434ba65a74e7`;
the Clang-built `libarchon_gated_backend.a` SHA-256 was
`8b72089c99f92c2675cac60509fbc5df1c6ff3a77830173eb9b6d5efd3d93147`.
These are recorded inputs, not requirements that builds on other hosts
produce byte-identical packages.

## Focused evidence

- The generated/fallback differential exercised 343 exact token and
  diagnostic cases, including all ten standard punctuation spellings.
- Both scanner builds executed the three standard-library operator and
  utility fixtures with Clang C11 and with address/undefined-behavior
  sanitizers on the emitted C and runtime. Both rejected the unselected
  operator and different nominal `Result` fixtures.
- The checked-HIR semantics test verifies that applications become ordinary
  calls, carry MIR effect order, and preserve the existing rules for
  specialization, caller loans, and unique owners. It exercises malformed
  source candidates and invalid unused declarations.
- Both scanner builds expanded an operator captured inside `quote`/`run`
  and another operator under two ellipsis ranks, then executed the emitted
  C11. This is not a claim that the scalar MIR evaluator admits staging.
- The LSP test suite passed 20 cases with each compiler; its operator
  navigation checks saved bytes and will not run declaration extensions
  or PPX.
- In both configurations the relocated compiler resolved all ten installed
  operators and executed the installed native `fold` processor on an
  operand of a custom operator. The generated configuration additionally
  emitted, linked with Clang, and ran the Archon AMD64 scalar fixture.

## Broad regression results

| Clang build | Passed | Failed | Skipped | Selected |
|---|---:|---:|---:|---:|
| Aurocks + AMD64 Archon | 276 | 5 | 1 | 282 |
| Forced fallback | 261 | 5 | 1 | 267 |

Both runs passed the focused operator integration, semantics, PPX and
relocated-install tests. The generated run also passed scanner parity and
the Archon native differential suite. Both broad runs skipped the cgroup
test; the GCC-specific toolchain probe was not selected.

After the final scanner and PPX spelling validation changes, the focused
rebuild and rerun passed **9/9 generated** and **8/8 fallback** (the added
generated test is scanner parity). These runs are logged as
`generated-head-ctest.log` and `fallback-head-ctest.log`.

The two runs had the same remaining failures. `on1x.contracts`,
`on1x.mulib` and `on1x.header_registration` hit the unchanged
`mulib-elvi-scrape.h` fingerprint drift. `on1x.nominal_hir` and
`on1x.container_local` were reproduced with pre-operator Clang binaries
(`invalid nominal HIR/wire admitted` and `unsupported_box_string`,
respectively). A repeat from the final compiler builds had the same
counts and failures. Its logs are `generated-release-ctest.log` and
`fallback-release-ctest.log` in `/dev/shm/on1x-operators/`.

## Coordination and evidence

The implementation assigns W1–W6 explicit source ownership. The live ledger is
`/dev/shm/on1x-operators/{TASKS,INTERFACES,DECISIONS,BLOCKERS,STATUS}.md`; workers
exchange messages in separate inbox/outbox directories. Ledger/message
updates take `coordination.lock` and replace a same-directory temporary file
atomically. Compiler artifacts live outside that shared-memory queue in
separate build directories.

## Follow-up qualification

The four follow-up workstreams use `/dev/shm/on1x-operators/followup/` for
their locked inboxes, outboxes, and qualification logs. The compiler now
runs the declarative `on1x/hir/On1xMETA/source/operators.o1m` program before
inference. The installed CLI test checks the `source_desugar` report with
optional optimizations disabled.

Using the same Clang commands above with new build directories, the
follow-up focused selection is:

```sh
ctest --test-dir "$dir" --parallel 8 --no-tests=error --output-on-failure \
  -R '^on1x\.(operators([.]|$)|macros$|local_syntax$|modules$|ppx$|stdppx|meta_hir$|processor(_semantic|[.])|formatter$|addon_lsp$|scanner_parity$|sdk\.(frontend|project))'
```

This selection covers generated operator declarations and fixity from
macros, one-pass PPX regrouping, no-execution formatter scope discovery,
SDK source-view projection, mandatory source desugaring, and installation.
The follow-up Clang builds passed **33/33 generated** (including 343 exact
scanner-parity cases) and **32/32 forced fallback**. The broad results
above describe the earlier implementation; the follow-up used the
focused selection.

As a separate backend compatibility check, the follow-up compiler emitted
saved MIR for a scalar custom operator. The prior Clang-built standalone
Archon provider consumed it, and the Clang-linked executable printed `42`.
This was a saved-MIR compatibility test, not a fresh Archon provider build.
