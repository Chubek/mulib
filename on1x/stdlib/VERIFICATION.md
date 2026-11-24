# Verification report

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
and the optional Cephyr companion source. It does not include
[`std.daemon`](daemon/README.md), which requires the separate KernelBind
build path. The ordinary library tests check that the daemon is rejected
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
