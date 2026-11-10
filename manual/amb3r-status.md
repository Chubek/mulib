# Amb3r engineering status — M0 repair log

## Baseline and scope

Baseline: `b6f240d440e20683698ee5ee525a86ec833b49ff`, initially clean.
Environment: Linux x86-64, kernel `7.2.6-arch2-1`, glibc 2.44,
GCC `16.2.1 20260810`, Clang `22.1.8`, CMake `4.4.3`.
The checkout contains 1,085 `include/mulib-*.h`, 375 `tests/test_*.c`
(hyphen-named suites are additional), and 1,086 `manual/mulib-*.md` files.

Scope: M0 inventory/dependency/profile mapping, existing EDA regression baseline,
and two demonstrated `eda-ids` defects. This is a bounded repair pass, not
completion of M0's behavioral audit or the DB-1/M1 acceptance gate.
No public function signatures, struct layouts or implementation switches change.

Machine-readable change inventory (runtime results are detailed under Evidence):

```json
{
  "baseline": "b6f240d440e20683698ee5ee525a86ec833b49ff",
  "initial_worktree": "clean",
  "milestone": "M0-first-repair-slice",
  "profile": "pool-local 64-bit typed IDs with 24-bit nonwrapping generations",
  "api_compatibility": "signatures/layouts/switches preserved; failed init now preserves destination",
  "regressions": ["failed-init destination mutation", "premature final-generation retirement"],
  "evidence_sizes": {
    "model_operations": 4096, "allocation_failure_positions": 6,
    "amb3r_c_suites": 87, "sanitizer_ctests": 649,
    "module_examples": 917, "composition_examples": 187,
    "focused_suites": 591, "combined_builds": 10, "isolated_header_builds": 3246
  },
  "numerical_evidence": "not applicable to integer identity/storage repair",
  "performance": "no throughput claim; first-fit O(length), validation/removal O(1)",
  "unsupported": ["persistent IDs", "cross-pool uniqueness", "typed DB graph", "atomic pool/DB transactions"],
  "next_dependency": "M0 behavioral gap audit before typed DB references",
  "affected_paths": [
    "include/mulib-eda-ids.h",
    "tests/test_eda_ids.c", "tests/test_eda_alloc.c", "tests/test_cpp_link.cpp",
    "examples/modules/eda-ids.c", "manual/mulib-eda-ids.md",
    "tools/audit_amb3r.py", "manual/amb3r-inventory.json", "manual/amb3r-status.md",
    "MANIFEST.yaml", "manual/strata.md", "tests/declarations.c",
    "tests/implementation.c", "tests/test_hardening.c", "tools/check.py",
    "mudsl/mull/bridge/Mulib/Generated.inc",
    "mudsl/mull/bridge/Mulib/Implementation.c",
    "mudsl/mull/bridge/Mulib/Inventory.json",
    "mudsl/tests/mull/mulib/eda-ids.mull",
    "mudsl/tests/mull/mulib/eda-ids.expected"
  ]
}
```

[Machine-readable source inventory](amb3r-inventory.json) records the 83 modules
included by `tests/amb3r_implementation.c`, plus the legacy ImGTK host. It records
source hashes, dependency edges, public type references, documented profiles,
source-level bounds and test references. These are extracted facts, not inferred
standards conformance or proof that every branch has been exercised. All 84 are
present with partial roadmap capabilities; no missing-module claims are inferred
from filenames. Regenerate/check with:

```sh
python3 tools/audit_amb3r.py --write
python3 tools/audit_amb3r.py --check
```

## Reviewed changes

### ID ownership and generation exhaustion

- `include/mulib-eda-ids.h`: initialize into a temporary pool, release temporary
  arrays on failure, and publish only after all three allocations succeed.
  Previously, a failed initialization overwrote the destination.
- Reuse an inactive slot at generation `0xffffff`. Previously the `<` comparison
  retired it one generation early. Removal advances to `0x1000000`, outside the
  encoded 24-bit range; subsequent creations skip it permanently.
- `tests/test_eda_ids.c`: final-generation issuance, retirement, wrong-kind/stale
  rejection, invalid arguments and a 4,096-step deterministic live/stale model.
  The boundary test seeds a valid near-exhaustion state rather than performing
  sixteen million cycles.
- `tests/test_eda_alloc.c`: fail each of three initialization and three growth
  allocations. Check byte-preserved destination, retained live IDs, unchanged
  output, cleanup and successful retry.
- `tests/test_cpp_link.cpp`: exercise the last generation and retirement across
  separately compiled C11 implementation / C++17 consumer objects.
- `examples/modules/eda-ids.c`: runnable ID → scalar DB → transaction fixture.
  Rollback preserves 1,000; commit publishes 2,200; deletion and slot reuse create
  a different ID whose record is 3,300, while lookup of the removed ID fails.
- `manual/mulib-eda-ids.md`: ownership, failure guarantees, pool-local identity,
  complexity, concurrency, limits and unsupported persistence contracts.
- `mudsl/tests/mull/mulib/eda-ids.{mull,expected}`: native binding lifecycle and
  stale-ID checks, shared by both MuLL hosts.

Initialization remains O(capacity), creation remains first-fit O(length) with
O(length) copying on growth, and validation/removal remain O(1). No new numerical
algorithm or throughput improvement is claimed. Storage is one uint32_t and two
uint8_t arrays per slot. IDs are unique only within a pool lifetime; reset or an
independent pool can reproduce the same numeric ID. The scalar DB still accepts
external IDs without consulting a pool. The example is caller-serialized glue,
not an atomic cross-pool/database transaction or persistent typed object graph.

### Baseline registration repair

Baseline `tools/stratify.py --check` failed because `parallel-sh` and `shebang-sh`
were omitted from its generated snapshot. Baseline MuLL inventory checking also
reported these two existing headers missing. Regeneration restores both entries.
The bridge now registers 1,085 headers/libraries, 4,869 functions, 7,954 native
types and 2,494 constants. `Generated.inc` has a large mechanical diff because
inserting existing shell types renumbers following native descriptors.

`tools/sync_inventory.py` also restores missing combined-test includes/switches
and manifest registrations for existing compiler examples and tests. Compiling
the newly included POSIX shell implementations exposed missing feature-test
setup in the combined test harness. `tests/implementation.c`,
`tests/test_hardening.c`, and generated combined/isolated TUs in `tools/check.py`
now request `_POSIX_C_SOURCE=200809L` before system headers, as required by those
modules' documented profile. Dependency implementation switches remain explicit
and application-owned. No shell implementation source was modified.

The checker additionally follows CMake's POSIX flags for shell-focused suites
and compile-only handling of `test_mune_compile.c` (which deliberately has no
linked Mune implementation). It exercises the two CLI examples with real inputs
and exact expected stdout: ordered `alpha/beta` jobs and `hello ${1}` expansion.
Calling these CLIs without arguments correctly returns usage error 2; the old
checker incorrectly treated that invocation as a failing functional example.
All eight shell runtime suites, the compile-only fixture and both CLI examples
passed strict GCC verification with the corrected setup.

## Actual data paths and gaps

| Existing path | Representation / ceiling | Next missing connection |
| --- | --- | --- |
| ID pool → scalar DB → snapshot transaction | Pool-local 8/24/32-bit ID; flat scalar records; copied snapshot | Typed reference validation and atomic graph changes |
| Schematic → text cards → SPICE netlist → transient → waveform | 256 two-terminal R/C/I/V components → 128 non-ground nodes / 1,024 parts → fixed-step backward Euler → finite double samples | Hierarchy, waveform sources, nonlinear models, adaptive rollback |
| HWIL → verifier → structural Verilog | 512-node, one-bit four-state combinational DAG | Typed sequential IR, source spans, HDL elaboration |
| AIG → ASCII AIGER | 1,023 nodes, complemented edges, single-output combinational writer | Explicit HWIL/AIG conversion, latches, mapping |
| COO → CSR/CSC → matvec / transpose | Dynamic double arrays; duplicate entries additive | Sparse factorization and symbolic reuse |
| PCB straight segments → DRC / Gerber | Double-coordinate centerlines, all-pairs clearance, restricted RS-274X | Track layer assignment, pads/vias, robust topology, independent CAM validation |
| Simulation events → heap operations | `(time, delta, region, order)` keys | Executing processes, NBA semantics, oscillation limits |
| ImGTK host + Lua widget library | 30 optional widgets; canvas/drawlist/plot helpers are separate | Transactional schematic editor, retained/culling scene index |

The existing vertical fixture exercises the schematic/transient/wave, PCB,
HWIL/Verilog and AIG/AIGER branches. It does not prove that a single persistent
design owns all those representations. It checks backward-Euler sample values;
it is not an adaptive-transient or mesh-refinement validation.

### Duplicate-responsibility decisions

| Facilities reviewed | Canonical responsibility and remaining work |
| --- | --- |
| `sat` / `formal-sat` / `smt` | `formal-*` owns hardware proof semantics. Existing `sat` is DPLL with up to 4,096 variables and temporary assumptions; `formal-sat` is separate 32-variable DPLL with UNKNOWN. A shared result/budget adapter and differential tests are still needed; neither is CDCL. |
| `linalg` / `num-dense` | Keep graphics float vectors/mat4/quaternions in `linalg`; numerical double matrix solving belongs to `num-*`. Their layouts and purposes differ. |
| `graph` / `eda-netgraph` / `ecad-connect` | Generic dense directed adjacency is not an electrical hypergraph. `eda-netgraph` stores indexed nets/terminals; `ecad-*` owns geometric-to-electrical connectivity, currently name-based only. |
| `geosolve` / `num-rootfind` / SPICE | Geometry residual solving is not a circuit device/Jacobian contract. `num-*` owns reusable numerics; `spice-*` owns MNA/device/conservation semantics. |
| `egraph` / `rewrite` / `synth-aig` | Generic untyped congruence/cost extraction differs from complemented Boolean networks. Reuse needs explicit typed conversions and equivalence checks. |
| `rf-microstrip` / `pcb-impedance` | Existing PCB convenience API delegates the approximate zero-thickness RF formula; keep that single implementation owner. |

## Evidence

Before source changes, `bash tests/run_amb3r_tests.sh` passed **87 C suites**,
C++ declarations and C/C++ linkage. The initial complete checker timed out at
120 seconds after reporting 917/917 module examples; a longer checker and CTest
run were interrupted by a server/host restart. Their temporary artifacts were
lost, so they are not counted as completed baseline checks. The initial Clang
sanitizer CMake build completed before the restart.

Both new regressions failed against the original implementation:
`retirement` asserted that the last-generation ID stayed in slot zero;
`ids_failure_atomicity` asserted byte-preserved fresh destination storage.
Both aborted with assertion failures before the repair.

Completed focused verification:

```sh
# Repeated for eda_ids, eda_alloc, eda_object, eda_design_db,
# eda_transaction and eda_undo, then each executable was run:
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  tests/test_eda_ids.c -lm -o /tmp/opencode/cc-test_eda_ids
clang -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -fsanitize=address,undefined -fno-sanitize-recover=all \
  -fno-omit-frame-pointer -Iinclude tests/test_eda_ids.c \
  -lm -o /tmp/opencode/clang-test_eda_ids

# Compile/run the integration example with both compilers:
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  examples/modules/eda-ids.c -lm -o /tmp/opencode/ids-example
/tmp/opencode/ids-example

CC=clang CXX=clang++ \
CFLAGS='-std=c11 -O1 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer' \
CXXFLAGS='-std=c++17 -O1 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer' \
TMPDIR=/tmp/opencode bash tests/run_amb3r_tests.sh
```

Results: all six focused suites pass with GCC and Clang ASan/UBSan; the example
passes with both compilers; the sanitizer Amb3r runner passes **87 C suites** and
both C++ gates. Allocation tests end with zero tracked live allocations.

Regeneration/check commands:

```sh
python3 tools/sync_inventory.py
python3 tools/stratify.py
python3 tools/manuals.py --write
python3 tools/manuals.py --check
python3 mudsl/scripts/generate_mulib.py --include include
python3 mudsl/scripts/check_mulib_inventory.py --include include
```

All completed successfully with the installed libclang bindings. MuLL validation:

```sh
cmake -S mudsl -B /tmp/opencode/mulib-mudsl -DMULL_ENABLE_MULIB=ON \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build /tmp/opencode/mulib-mudsl --target mull_tests mull_d_test_build -j 2
ctest --test-dir /tmp/opencode/mulib-mudsl \
  -R 'mull_(host|conformance|mulib_inventory)' --output-on-failure
```

Host C++, host D, inventory and inventory regression tests passed. Both optimized
and unoptimized conformance suites each passed **80 fixtures/book examples on
two runtimes**, after correcting the new fixture's
expected output to include the harness's standard `=> ` display prefix.

Full CMake verification:

```sh
cmake -S . -B /tmp/opencode/mulib-build-amb3r -DMULIB_SANITIZERS=ON \
  -DCMAKE_C_COMPILER=clang
cmake --build /tmp/opencode/mulib-build-amb3r -j 2
ctest --test-dir /tmp/opencode/mulib-build-amb3r --output-on-failure -j 2
```

After fixing the feature-test macro ordering described above, build succeeded
and **649/649 tests passed**, 105.38 seconds reported wall time. This includes
`test_hardening`, `test_hostile_inputs`, the EDA fixtures, the ImGTK legacy host
suite and the separate C/C++ linkage fixture. This timing describes this test
run, not a library performance benchmark. Two checker runs using superseded
harness configuration were stopped before completion; neither is counted as a
pass. A completed checker run then exposed an overly broad basename match in
the new CLI setup: it also applied to the unrelated thread-pool example
`examples/modules/parallel.c`. Restricting CLI configuration to exact source
paths fixed that harness regression. The final full run succeeded:

```sh
python3 -u tools/check.py -j 4
```

| Checker category | Result |
| --- | --- |
| Module examples | 917/917 passed |
| Composition examples | 187/187 passed |
| Focused suites (including the documented compile-only fixture) | 591/591 passed |
| Combined builds / include-order checks | 10/10 passed |
| Isolated C11/C++17 header builds | 3,246/3,246 passed |
| Header inventory | 1,082 implemented registration entries; three support headers excluded by checker policy |

The first slice's final source inventory check, manual references, bridge
registration and `git diff --check` also passed. Its change-inventory JSON
accounts for the 20 paths affected at that point. No unresolved failure from
that slice's executed validation remained.

## Progress / handoff

| Module / scope | Owner | Baseline SHA | Documented profile | Tests executed | Acceptance gate | State | Unverified behavior | Next dependency |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `eda-ids` | eda | `b6f240d` | Pool-local generation-checked handles | GCC, Clang ASan/UBSan, fault injection, model, C++/MuLL | Lifetime/retirement repair | implemented-profile | Full 32-bit slot capacity; persistence; cross-pool identity | Typed DB reference ownership |
| 83-module slice | domain owners in JSON | `b6f240d` | Existing limited profiles | 87 C suites + C++ gates | M0 smoke baseline | existing-limited | Comprehensive independent numerical/format validation | Complete M0 behavioral gap audit |
| Shared registrations | tooling | `b6f240d` | Full present-header index | Generators, complete checker, 649 CTests, MuLL hosts | M0 build metadata | integration-validated | Platforms outside this Linux configuration | Complete M0 behavioral gap audit |

NEXT: finish the M0 behavioral gap audit using this reproducible passing baseline;
then implement typed DB references and atomic graph edits before persistence/UI
expansion. DB-1, SPICE-1/2/3, HDL, manufacturing, FPGA-device, ASIC-signoff and GUI
scale acceptance gates are not claimed by this pass.

## Second repair slice — snapshot transaction ownership

BASELINE: same commit `b6f240d440e20683698ee5ee525a86ec833b49ff`, dirty with
the first slice's 20 preserved paths. The prior full-check baseline was passing;
the new pre-edit checker invocation also completed successfully. Platform and
compiler versions remain those recorded above. No branch/commit change occurred.

SCOPE: M0 audit of `eda-design-db` / `eda-transaction`, with an in-place repair
to transaction begin and focused coverage of existing commit/rollback behavior.

```json
{
  "baseline": "b6f240d440e20683698ee5ee525a86ec833b49ff",
  "initial_worktree": "dirty: first M0 slice retained",
  "milestone": "M0-snapshot-transaction-repair",
  "profile": "caller-serialized owned snapshot over flat finite-scalar design DB",
  "api": "existing symbols/layouts/switches preserved; failed begin preserves both objects",
  "regressions": [
    "begin allocation failure overwrote inactive transaction state",
    "begin accepted its own staged store as target",
    "begin overwrote inactive owned staged storage"
  ],
  "algorithm": "clone into local owned store, publish only on success; existing epoch-checked swap on commit",
  "complexity": "begin O(record count), one nonempty snapshot allocation; commit allocation-free",
  "validation": {
    "state": "passed",
    "sanitizer_ctests": 649,
    "amb3r_c_suites": 87,
    "mull_fixtures_per_mode": 81,
    "mull_hosts": ["C++", "D"],
    "module_examples": 917,
    "composition_examples": 187,
    "focused_suites": 591,
    "combined_builds": 10,
    "isolated_header_builds": 3246
  },
  "numerical_evidence": "not applicable; no numerical algorithm changed",
  "unsupported": ["nested transactions", "cross-DB atomicity", "persistent journals", "concurrent unsynchronized access", "target reset/ABA detection"],
  "next_dependency": "typed DB reference and lifetime contract after M0 behavioral review",
  "affected_paths": [
    "include/mulib-eda-transaction.h",
    "manual/mulib-eda-transaction.md",
    "examples/modules/eda-transaction.c",
    "tests/test_eda_transaction.c",
    "tests/test_eda_alloc.c",
    "tests/test_cpp_link.cpp",
    "mudsl/tests/mull/mulib/eda-transaction.mull",
    "mudsl/tests/mull/mulib/eda-transaction.expected",
    "MANIFEST.yaml",
    "mudsl/mull/bridge/Mulib/Inventory.json",
    "manual/amb3r-inventory.json",
    "manual/amb3r-status.md"
  ]
}
```

CHANGED / ALGORITHMS: `begin` now rejects self-targeting and nonempty inactive
staged ownership before mutation. It uses `mulib_eda_db_clone` rather than its
own duplicate allocation/copy implementation, and only installs target, staged
store, epoch and activity after cloning succeeds. No dependent implementation
switch is enabled internally. Commit still publishes all staged edits with one
epoch increment; failed conflict/exhaustion commits retain the active snapshot.

API: Doxygen and the module manual now specify initialization, target lifetime,
exclusive ownership, pointer invalidation, failure preservation, reuse, allocation
behavior, source-shape checks, epoch limits and boolean-status limitations.
The integration snippet now explicitly supplies the existing DB implementation
dependency. Target reset/free while active is prohibited: the current epoch
token does not encode database lifetime and cannot detect ABA reuse.

EVIDENCE: before the repair, `test_eda_transaction` aborted on accepting
`&t.staged` as its own target, and `test_eda_alloc` aborted when failed begin
changed the transaction's bytes. After repair, strict GCC and Clang ASan/UBSan
passed these five suites: `eda_transaction`, `eda_alloc`, `eda_design_db`,
`eda_undo`, `eda_extended`. Both compilers also ran the `eda-transaction` and
`eda-ids` examples. Commands use the first slice's focused compiler flags.
Allocation coverage fails the single snapshot allocation and staged growth,
then retries successfully and commits with further allocations disabled;
an empty snapshot also begins/commits with allocations disabled. Tracked live
allocations return to zero.

The focused transaction suite additionally verifies grouped update/delete/insert,
rollback/reuse, preserved old snapshots, external-write and two-snapshot conflicts,
empty/unedited commit invalidation, invalid source shape, NULL arguments and
UINT64_MAX epoch exhaustion. The **87-suite** Clang sanitizer Amb3r runner and
its C11-object/C++17-consumer gate passed. The updated C++ fixture verifies
self-target rejection plus scalar rollback and publication.

INTEGRATION: the standalone transaction example atomically replaces scalar
record 1's value 5→7, deletes record 2 and inserts record 3 with value 20,
publishing just one epoch. An external update to 9 defeats a stale commit;
its retained snapshot remains 7 until rollback. The MuLL fixture exercises
the same ownership lifecycle through both C++ and D hosts; optimized and
unoptimized conformance tests passed, now including 81 fixtures/book examples.
The bridge inventory check initially overlapped regeneration after a Doxygen
correction and saw a stale fingerprint; it passed when rerun after regeneration.

REPRODUCE:

```sh
python3 tools/sync_inventory.py
python3 tools/stratify.py
python3 tools/manuals.py --write
python3 mudsl/scripts/generate_mulib.py --include include
python3 mudsl/scripts/check_mulib_inventory.py --include include
python3 tools/audit_amb3r.py --write
# Reuse the first slice's sanitizer-enabled CMake configuration:
cmake --build /tmp/opencode/mulib-build-amb3r -j 2
ctest --test-dir /tmp/opencode/mulib-build-amb3r --output-on-failure -j 2
cmake --build /tmp/opencode/mulib-mudsl --target mull_tests mull_d_test_build -j 2
ctest --test-dir /tmp/opencode/mulib-mudsl \
  -R 'mull_(host|conformance|mulib_inventory)' --output-on-failure
python3 -u tools/check.py -j 4
```

The sanitizer-enabled full CMake build succeeded and **649/649 CTests passed**
(89.99 seconds reported wall time). The final standalone checker completed
successfully with **917/917 module examples, 187/187 composition examples,
591/591 focused suites, 10/10 combined builds and 3,246/3,246 isolated header
builds**. Final manual references, dependency strata, source inventory, MuLL
inventory and whitespace checks pass. Both slices' machine-readable inventories
together account for all 26 currently affected paths. No validation jobs or
unresolved failures remain for this slice.

| Module | Owner | Baseline SHA | Documented profile | Tests executed | Acceptance gate | State | Unverified behavior | Next dependency |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `eda-transaction` | eda | `b6f240d` + first slice | Caller-serialized scalar snapshot | GCC, Clang sanitizers, allocation injection, C++ ABI, MuLL C++/D, 649 CTests | M0 begin ownership and publication | implemented-profile | Arbitrary forged metadata; target lifetime reset; persistence | Typed DB reference ownership |

LIMITATIONS: this remains a scalar snapshot transaction, not the typed graph,
persistence, migration, reference validation or cross-representation undo required
by DB-1. It supplies no synchronization and does not validate arbitrary forged
public metadata. No new performance or standards-conformance claim is made.

NEXT: review typed reference/lifetime ownership in the existing DB adapters;
the single highest-value dependency for M1 remains a validated typed object graph.
