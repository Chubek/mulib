# Verification report

## Elvi, HTML, Kron, Daemon and HTTP Probe — 2026-11-26

The [offline example](../examples/http-probe/README.md) builds real generated
C11 and exercises script-based and manual Probes against two in-memory HTTP
fixtures. It checks exact raw bytes, normalized HTML and metadata, a named
procedure returning text and a capturing closure returning `i64`. The same
application runs as a supervised child in immediate, asynchronous, and
durably scheduled execution. Fake clock steps exercise admission, reopening,
current dispatch policy, a removed runner, and recurrence cancellation.

The focused fixtures also cover invalid ElviScript and HTML, parser/handler
type mismatches, borrowed receiver escape into a handler, missing native
authorization at link time, denied URL and cookie stages, rejected response
metadata, allocation/descriptor exhaustion, malformed persisted descriptors,
failed child startup/exit, and uncertain scheduler commits. The library source
bundle and relocated SDK tests compile and run the composed fixture using
their selected module and native resource trees.

The 11 focused CTests below passed, including the installed and source-bundle
consumers; the composed strict-C11 runner also passed with GCC 16.2.1 and
Clang 22.1.8 ASan/UBSan. Six existing C kernel regressions passed (Elvi
provider, parser, script and transport, KronVM, and daemon/fswatch).

Reproduce the Linux x86-64 host profile (Unix Makefiles, Clang 22.1.8,
LibStone `76d24a7f8b19294bab2b713dc69f799e412b62d2`, Release, fallback scanner).
`CEPHYR_SO` must point to the matching **build-host** Cephyr shared library
used by FFI recipe generation; the runtime Cephyr backend is not used:

```sh
cmake -S on1x -B build/on1x-probe -G 'Unix Makefiles' \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DON1X_USE_FALLBACK_SCANNER=ON -DON1X_BUILD_NATIVE_PPX=OFF \
  -DON1X_BUILD_EXTENSION_SDK=OFF -DON1X_WITH_SYREN=OFF \
  -DON1X_WITH_WETSTONE=OFF -DON1X_WITH_PROJECT_QMR=OFF \
  -DON1X_STDFFI_CEPHYR_LIBRARY="$CEPHYR_SO"
cmake --build build/on1x-probe --parallel 4
ctest --test-dir build/on1x-probe --output-on-failure \
  -R '^on1x\.(stdlib\.(elvi|html|http|http_probe(_context|_components)?|kron_native|daemon_supervisor(_unsupported)?)|stdlib_project|install)$'
python3 -B on1x/tests/test_http_probe.py --on1xc build/on1x-probe/on1xc \
  --cc clang --scratch build/on1x-probe --package-tests \
  --cflag=-fsanitize=address,undefined --cflag=-fno-sanitize-recover=all --cflag=-g
```

MuLIB baseline: `fa514d281b541a0aab2b217ef80b426fbbd17ae2`, plus the
changes in this tree. The changed kernel inputs had these SHA-256 hashes:

```text
6a3d5f006ff2848785c6f9296e860b7d4651c2e2bb0340bdd197e8513d4c02a6  kernels/mulib-daemon.h
cccfdd9f82a54b93087d4c2900701ab9a17b690974f97cefe7623431794d84cf  kernels/mulib-elvi-scrape.h
f84129a1b61b6fec6d159dbe8af0f5fa38acae59ca47ab16edc6f410c8eea4f4  kernels/mulib-elvi-script-transport.h
e51ac63754cae1e547e50c11f8f34e6f6538dc904db793e068e473d7e0c14d42  kernels/mulib-kronvm.h
```

Only the explicit native Linux C11 profile is qualified by these integration
tests. TLS/HTTP framing in the example uses a synthetic peer with no sockets;
it is not a real certificate, DNS or egress-policy test. The separate
`on1x.stdlib.http` regression uses local TLS. HTTP/2 and HTTP/3, arbitrary
script substitution, persisted code/closures, automatic credentials, and
cross-process On1x result ownership are not qualified. These checks do not
admit a safe foreign ABI or establish SML Basis conformance. The companion
libraries retain existing KernelBind IPC and ElviScript Lua/discard behavior
as separate profiles. See the [compiler evidence](../docs/probe-c11-qualification.md).

## ListPair mapping — 2026-11-24

`on1x.libraries` compiles and executes the `basis_listpair_map` fixture through
the direct C11 backend. It checks the shortest-input rule in both directions,
equal-length mapping, empty inputs, an empty-list element, callback/result
order, and repeated mapping with a capturing callback under a 128-live-node
limit. Five subprocess fixtures check fatal length and root-type errors,
including the observable callback prefix before each failure. `on1x.install`
exercises `basis_listpair_map_eq` through the relocated standard library.

On Linux x86-64 with GCC 16.2.1, the generated scanner, and pinned LibStone
`3be5e9d7`, `on1x.libraries`, `on1x.modules` and `on1x.stdlib_project` passed.
`on1x.install` passed after running the Makefiles `preinstall` target.
All six emitted ListPair C11 fixtures also passed with Clang 22.1.8 ASan/UBSan;
leak detection was enabled on success and disabled on intentional fatal exits.

Run with `ctest --test-dir BUILD -R '^on1x\.(libraries|install)$'
--output-on-failure`. For Unix Makefiles, run `cmake --build BUILD --target
preinstall` first to produce the SDK shared library's install-time relink
artifact. This checks the On1x adaptation, not the whole SML `LIST_PAIR`
signature or SML exception semantics.

## Restricted project package and process environment — 2026-11-23

The restricted source package includes [`std.environ`](environ/README.md)
and the optional Cephyr companion source. It does not include the legacy
[`std.daemon`](daemon/README.md) IPC entry point, which requires the separate
KernelBind build path. The ordinary library tests check that IPC is rejected
without KernelBind and that it emits C under the intended profile.

## Restricted project package — 2026-11-18

`tests/test_stdlib_project.py` (under `on1x/`) builds and relocates a package
containing all 129 declared library modules, the Basis and prettyprint C
companions, and the existing standalone `mulib` discovery manifest. It verifies
the packaged bytes and snapshot hashes, rebuilds byte-identical package files,
and checks that removing the pervasive declaration fails rather than falling
back to the installed stdlib. A separate On1x executable uses the relocated
prelude, root List facade, parent-relative `base/list` module, raw Basis
arithmetic, prettyprint, INI and `std.mulib` metadata. INI and MuLIB metadata
link against the SDK companions, not copies from the package.

Reproduce on Linux with `python3 on1x/tests/test_stdlib_project.py --on1xc
build/on1x/on1xc --cc cc`. The test also checks that undeclared paths, path
escapes, cross-package relative imports, symlinks, and unregistered resources
do not publish an output. Packaging does not validate foreign ownership or
increase Basis conformance.

## Extension integration — 2026-11-15

The current inventory has **118 library modules and six On1x test programs**,
with all modules admitted by the actual compiler. The SML Basis inventory stays
at **186 entries / 60 partial bindings**: no conformance status was promoted.
See the newest matrix in [`../VERIFICATION.md`](../VERIFICATION.md).

Executed additions:

- Both shipped `pure`/`parsec` programs, checked Option/Result tags and callback
  short-circuiting, map lookup/removal, malformed graphs, deduplicated cyclic BFS,
  signed scanning at i64 boundaries and snapshot-free search loops.
- Real canonical MuLIB JSON/YAML/TOML/INI and BLAKE2s/BLAKE3 through the automatic
  one-kernel-TU bridge plan. Tests compare hash output with Python's BLAKE2s
  implementation and the BLAKE3 empty-input vector. Malformed data, non-array
  JSON, missing/empty INI values, duplicate keys and invalid document suffixes
  exercise recoverable status rather than unsupported resource-return ABIs.
- Independent compilation of JSON/YAML/TOML/INI/crypto/socket/Unix groups and the
  status-only bridge. Counted injected failures cover temporary-copy allocation,
  snapshot allocation, bounded pool exhaustion and process-exit cleanup.
- Local-only numeric-IPv4 socket client/server execution: partial-send capable
  offset API, binary hex receive including NUL/0xff, text-NUL rejection, EOF,
  bad offsets, double close and stale handles after slot reuse.
- Real Exotract/ExoML discovery and a locked archive consumer using JSON/INI/hash
  libraries without manual adapter selection or duplicate kernel definitions.
- Installed INI adapter/consumer execution after prefix relocation, alongside
  the existing Basis/PPX and `std.mulib` installation regressions.

The new bridge manual is [`_bridge/README.md`](_bridge/README.md). These remain
trusted raw adapters: no SML/Base/Batteries conformance, compiler-native ADTs,
general foreign-owner/record/slice/f64 ABI or independently reviewed safe import
is claimed. Socket calls may block and use no TLS; no external service is used
by the tests. The previous report below retains its original snapshot/counts.

## Integrated compiler verification — 2026-11-15

The complete On1x suites pass with GCC generated scanning (**34/34**), Clang
fallback scanning (**33/33**) and Clang generated scanning with ASan/UBSan
(**34/34**). See [build configurations and reproduction](../VERIFICATION.md).

`on1x.libraries`, `on1x.modules`, `on1x.loader_api` and `on1x.install` verify:

- Actual `on1xc` admission of all **81 library modules**, including the original
  S-expression/visitor/PPX libraries, facade modules and pervasive helpers.
- Compilation, strict C11 linking and execution of the four On1x Basis programs
  (`core`, `collections`, `real`, `io`) and three C adapter programs.
- 71 signed floor-division/modulo pairs checked against Python integer arithmetic;
  minimum-i64 remainder, sign classification, decimal parsing and Word8 bounds.
- Generic Option/Functional helpers, ordered list callbacks, stable partition,
  improper-list rejection and bounded substring errors.
- Repeated byte searches under a small snapshot pool, native snapshot accounting,
  array allocation failure and temporary-buffer cleanup on binary snapshot failure.
- Namespaced/relative/quoted imports, aliases, prelude shadowing/overrides,
  automatic adapter linking and an independently compiled relocated-install client.

The catalog contains **186 entries** and inventories **85 `.on1x` files**:
81 library modules and four test sources. `catalog/refresh.py` maintains relocated
paths, source counts and `FILE_HASHES.json`; `tests/check_package.py` checks the
inventory and source/import consistency. Compatibility statuses remain partial.

This matrix does not establish SML Basis conformance, general foreign ownership
proofs, or execution of metadata-only optional/platform/functor entries.

## Artifact-production baseline (historical)

Generated: 2026-11-15

Successful validation during this package build:

- Static catalog/import/name/balanced-delimiter checks: **PASS** (186 catalog entries; 71 `.on1x` files, of which 67 are library modules and 4 are test source files).
- GCC `-std=c11 -Wall -Wextra -Werror -pedantic`: **PASS** for native adapter and three C test programs.
- Clang `-std=c11 -Wall -Wextra -Werror -pedantic`: **PASS** for native adapter and three C test programs.
- GCC AddressSanitizer + UndefinedBehaviorSanitizer native test suite: **PASS**.
- Negative tests check failed/expired handles, out-of-bounds, invalid array lengths and real-number domains: **PASS**.

Not verified in that earlier artifact-only environment (compiler/linking gaps
are covered by the integrated run above):

- Running the On1x compiler (`on1xc`) on the `.on1x` sources. Its repository checkout/toolchain is not available in the build container.
- End-to-end linking generated On1x/C code and the native adapter.
- SML Basis Library conformance or semantic equivalence.
- All optional platform integrations, networking, functors, ABI/ownership safety or other catalog-only components.

The package is an honest **partial implementation plus a complete named inventory**. It is not a fully implemented SML Basis Library.
