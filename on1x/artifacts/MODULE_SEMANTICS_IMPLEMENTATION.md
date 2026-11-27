# Module semantics implementation qualification

## Qualified revisions and assets

The combined hardening run starts at MuLIB `58f109f80aa8e5e805f010db1866738724475068`,
which integrates the original four workers. It includes native-link fix
`e5ceff82`, nested transparent views `ad33e439`, and graph hardening `35e0a613`,
plus the test registrations and aggregate-budget regression accompanying this
report. In the qualification worktree, the first two commits are equivalent
cherry-picks `39b746f8` and `018a3bc6`; parent integration uses `73685b1c` and
`03e92edc`. They are not different implementations.

Actual external inputs:

* LibStone `4607ce58ea70482a9c057c64557260329d36afde`.
* Explicit trusted external MuLIB checkout
  `cbb7d66f217f188493f25b38c10101aa26469c48`, stdlib Git tree
  `bb7e7814635bb6e161e4c092b2129c493ec89a88`.
* Build-host Cephyr `/mnt/nvme/mulib/build/on1x-mod-lanes/cephyr-pin/libcephyr.so`.
* GCC 16.2.1; Python 3.14.7; Debug compiler build, with assertions enabled.
* Embedded OML SHA-256
  `92457cf18bdaa4fef2c713615bbfe526e55f0152b51ee6cdba346724d89165b4`.
  CMake checks the pinned LibDSL header and OML digests.

No deleted stdlib, examples or docs were restored. The external profile executes
the explicitly trusted checkout's formatter planner at build time; a project's
`--stdlib` flag does **not** grant authority to import Python helpers. Private
coordination `/dev/shm/on1x-module-hardening-LiQC5GMt` is not a compiler input.

## Combined-tree build and executed evidence

From the isolated worktree `/mnt/warble/tempdir/opencode/module-hardening-debug`:

```sh
BUILD=/mnt/warble/tempdir/opencode/module-hardening-debug-build
STDLIB=/mnt/nvme/fpnext.mI6yYw/mulib/on1x/stdlib
cmake -S on1x -B "$BUILD" \
  -DON1X_MODULE_STDLIB_ROOT="$STDLIB" \
  -DON1X_LIBSTONE_ROOT=/mnt/nvme/libstone \
  -DON1X_STDFFI_CEPHYR_LIBRARY=/mnt/nvme/mulib/build/on1x-mod-lanes/cephyr-pin/libcephyr.so \
  -DON1X_WITH_WETSTONE=OFF -DON1X_WITH_SYREN=OFF \
  -DON1X_WITH_PROJECT_QMR=OFF -DON1X_WITH_KERNEL_NATIVE=OFF \
  -DON1X_BUILD_DYNAMIC_SDK_PLUGINS=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build "$BUILD" -j4
ctest --test-dir "$BUILD" --output-on-failure
```

The full compiler, real generated Aurocks/ASDL frontend, SDK and VM runner built.
The combined run passed **16/16 CTests in 12.67 seconds**: algebra, transparent
permit, graph/reference model, wire round-trip, PPX metadata, loader, signature
API, opaque direct-C11 execution, signature CLI, nested transparent CLI, project
membership, native linking, SDK logistics, graph driver, graph packet and LSP.
The LSP test ran 21 cases, with two explicitly skipped because Syren was disabled
(import diagnostics and the Syren/foreign-library smoke). It is not a complete
LSP or SDK regression qualification.

Additional executed checks:

* All **11 self-contained** `Modules` cases in `tests/test_module_acceptance.py`
  passed through the combined real compiler, project driver, C compiler/linker
  and executable. This includes private/shared access, module fragments and
  interface association, explicit program admission, opaque provider identity,
  MIR/VM rejection and preservation of prior project artifacts.
* The twelfth case, `test_energy_budget_legacy_and_sealed_portable_backends`,
  was run explicitly and **errored** with `FileNotFoundError` for the deleted
  `on1x/examples/energy-budget/expected.txt`; it was not silently skipped.
* `test_module_signature_cli.py` and `test_nested_transparent_cli.py` both passed
  with generated C compiled as strict C11, `-Wall -Wextra -Wpedantic -Werror`,
  ASan and UBSan. Scalar/transparent transport prints `42`; the nested fixture
  prints three `42` lines. The compiler itself was not sanitizer-instrumented.
* `test_transparent_permit.cpp` passed independently with
  `-Wall -Wextra -Werror -fsanitize=address,undefined`.
* `test_module_graph.cpp` plus `test_module_graph_wire.py` passed with ASan/UBSan:
  500 seeded graphs checked against independent Floyd–Warshall reachability,
  SCCs, both closures, dependency order, cycle witnesses, malformed identities
  and bounds, maximum 1,024-node chain, deterministic JSON/sexpr round-trip.
* The real driver rejected a 17-file/17,000-import fixture below each file's
  parser limit but above the aggregate import-edge budget, preserving prior
  output. Its observational failure packet contains diagnostics, not nodes.
* 27 SDK headers compiled independently as C11 and C++20.
* A separate Debug `ON1X_MODULE_CORE_ONLY=ON` build passed all four CTests,
  including a repeat after integrating the semantic increment. Installation,
  prefix relocation and a consumer linked only against the installed OML
  archive/headers passed. This qualifies core relocation, **not** full SDK or
  compiler installation.

Reproduce the sanitizer CLI checks:

```sh
ON1X_TEST_VIEW_CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  python3 -B on1x/tests/test_module_signature_cli.py "$BUILD/on1xc" "$BUILD/signature-sanitized"
ON1X_TEST_VIEW_CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  python3 -B on1x/tests/test_nested_transparent_cli.py "$BUILD/on1xc" "$BUILD/nested-sanitized"
python3 -B on1x/sdk/tests/frontend/header_compile.py cc c++ on1x/sdk/include "$BUILD"
```

The initial full-build job hit a harness timeout and was continued. During
development, editing the graph API while compilation was active caused stale
objects to fail linking; a later edit referenced an option before declaration.
Both were corrected and rebuilt; the successful final run is the evidence
above, not those interrupted/failed development builds.

## Feature and capability matrix

| Feature | Implemented and executed scope | Exclusions/status |
|---|---|---|
| Immutable import and logical dependency graphs | Real loader snapshot, fragment membership, all request witnesses, separate internal/auxiliary relations, reverse closure/SCC/order; CLI JSON, sexpr, DOT; project v3 with v2 compatibility | Resolved observations, not execution authority. No partial SCC recovery or authoritative decoder. Package identities are canonical-origin scoped, not complete package/lock identities. |
| OML/LibDSL | Pinned synthesis, bounded attribute-based access; finite binding selection/projection/rename/merge; loader access/view decisions; installed archive relocation | Partial migration only. Resolver, full signature/seal/dependency/functor judgments are not OML-authored. Not the complete normative DSL contract. |
| Existing signatures and opaque modules | Actual-provider conformance; original closed-copy monomorphic direct-C11 opaque profile; visibility/alias/peer-identity and projection negatives | Advanced named/parameterized signatures, includes and equations can be declaration-only. A conformance result does not admit execution. |
| Transparent and manifest views | Same-provider monomorphic records with scalar leaves; nested exported records; scalar manifest equations; real C11 calls, construction, copying and projection | No managed/generic/recursive transport, structural tuple/array fields or mixed manifest/nominal executable signatures. Every reachable nested nominal must be explicitly exported. |
| Backend gates | Direct C11 profiles tested; unsupported MIR/VM routes reject before replacing artifacts | No expanded native/Archon backend parity; no managed cleanup ABI added. |
| Static functors | None | Unimplemented; no executable identity/substitution evidence. |
| Separate compilation | None | No checked ABI manifest, provider object reuse or consumer-without-provider-body proof. Combined Unit remains the execution path. |
| PPX graph protocol | Host-owned versioned read-only packet export, bounded wire/parser checks; graph inspection launches no PPX; Unit-v1 fail-closed tests | No negotiated module-aware transform/readmission, graph-changing PPX or new dependency authority. |
| SDK/LSP | Opt-in immutable graph JSON, source-overlay refresh with old snapshots retained; narrow saved-entry LSP graph smoke | No general module declaration-ID go-to-definition or full overlay-aware IDE resolver; disabled Syren cases noted above. |
| Project/native publication | Compiler-derived quotient report, staged membership validation; kernel-free direct-C11 project executables and atomic failure tests | Source-kernel/load-time housekeeping remains explicitly gated; not general native backend qualification. |

## Trust boundaries and hardening changes

Graph validation now rejects ID/map-key mismatches, unknown kinds, empty
providers, package/fragment contradictions, duplicate roots/physical targets,
dangling auxiliary inputs and advertised collection-limit violations before
traversal. Bounds include 1,024 logical/physical nodes, 16,384 requests and
65,536 physical witnesses; OML environments cap at 4,096 entries. Nested copy
representation traversal has 16,384 visits and depth 128. These are bounded
profiles, not a proof that every compiler operation is resource-bounded.

A source-loss bug was reproduced against the previous real graph driver:
`--emit-module-graph ENTRY -o ENTRY` succeeded and replaced the source with JSON.
The loader now excludes the graph destination from visited source/interface
inputs; the CLI also protects the entry, explicit interfaces and membership
index. The exclusion remains fatal under `--graph-partial`. Real tests cover
entry/provider preservation, a provider discovered before another missing
import, symlink/FIFO destinations, temporary cleanup and prior-output retention.
Inspection rejects conflicting checker/backend options.

Kernel-free native linking no longer imports a deleted housekeeping helper just
to discover unused include paths. A malicious helper under a selected source
library was not executed. Real linker failures and unavailable kernel profiles
preserved the previous executable. See [MODULE_NATIVE_LINK.md](MODULE_NATIVE_LINK.md).

## Remaining gates and historical evidence

The clean default compiler profile still cannot build without removed assets;
the explicit external-root profile is intentional, not a restored stdlib.
General stdlib/examples/native-PPX regressions are not registered by this narrow
profile and are not certified. Full compiler installation, cross-target/native
parity, managed opaque cleanup, static functors, genuine separate compilation
and negotiated module-aware PPX remain open. G3/G5 are therefore incomplete;
there is no production-readiness or full-contract-completion claim.

Earlier standalone and worker-specific results remain in
[MODULE_ALGEBRA.md](MODULE_ALGEBRA.md), [MODULE_GRAPH.md](MODULE_GRAPH.md),
[MODULE_TYPE_VIEWS.md](MODULE_TYPE_VIEWS.md) and
[MODULE_GRAPH_TOOLING.md](MODULE_GRAPH_TOOLING.md). The combined run above
supersedes their historical statements that integrated execution and ordinary
project C11 publication were unqualified. It does not supersede their explicit
feature exclusions.
