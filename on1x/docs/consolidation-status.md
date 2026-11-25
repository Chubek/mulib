# On1x consolidation ledger

This ledger records the first integration increment based on MuLIB
`167286b84830f019e3c19fc9d9eef8b9e3ce6c59` and LibStone
`ac95160900398cb864a91b134fc6d716b29c0a28`. The primary worktree was clean at
entry; its revision matched the earlier review and `origin/master`. Other
local branches and worktrees were not substituted for the active source.
The existing [profile reference](profile-reference.md) describes the boundary
between checked source, MIR, and executable products.

## Verified scope and remaining work

- **MIR verification — executable v1–v4:** Direct calls compare their exact
  argument types in place after arity checking; even a forward reference
  cannot cause an unbudgeted traversal of a wide callee signature. Negative
  tests cover arity/type mismatch, foreign-function slots and loop
  first-iteration initialization. No source or wire ownership widening.
- **Source/VM differential — verified for the fixture:** Direct C11 and
  ordinary and optimized saved VM artifacts match a fixed trace and status
  for copy-only nominal `Result<i64,i64>` and `Result<bool,i64>`. Tests include
  a success, propagation from a call argument, and propagation from a
  short-circuit RHS. Scalar MIR modes still reject and preserve the output
  sentinel. VM's projection erases nominal identity; this says nothing
  about owner-bearing `Result` or independent nominal MIR validation.
  O1VM encoding and decoding share the same explicit v1–v3 allowlist.
  Portable MIR v4 must be specialized explicitly before encoding; the
  writer rejects unsupported versions before publication. Saved-MIR
  conformance also checks canonical round trips, C11 and VM production
  from saved inputs, and preservation of earlier artifacts on failure.
- **Portable FFI — executable for a narrow additional proof case:**
  The second, independent FFISpec check can use an exact normalized
  equality established on the current branch to prove a single-parameter
  affine signed-i64 bound using the peer's direct interval. The lookup
  spends from its existing work budget. Optional bounds, the non-equal
  branch, and bounds not proved for all inputs still reject. FFIHandler v1,
  runtime linkage, and native admission are unchanged. The FFI worker
  compared six endpoint/equality cases through an isolated generated
  recipe using `--run-ir` against its C definition. Independent review
  reproduced generation and table compilation, not that client run.
  At baseline, the
  real Cephyr scan found 2,071 headers: 1,748 missing views, 322 unsupported
  and one generated kernel (`clamp`, three callable exports). Those counts
  are for these roots and this configuration, not supported-kernel counts
  for all installations.
  The standard FFI generator now consistently uses the configured Cephyr
  resource root. A fresh source archive without a populated vendored
  checkout generated and compiled the clamp recipe and passed both
  generator/tool CTests using external LibStone. This focused build used
  `MULIB_KERNELS_PATH` containing only a copy of `kernels/mulib-clamp.h`.
- **Project SDK validation — repaired:** `on1x-sdk validate lock` uses the
  canonical parser and reports the exact validated v1–v4 schema, with no
  capability grant. The SDK install component ships the two missing
  project-driver helpers; the relocation test covers both an On1xfile
  and a v4 lock under a shadowing `PYTHONPATH`. This is not a unified
  backend capability inventory.
- **Explicit CLI checker admission — repaired:** Unchecked inspection
  commands reject required Chisel/Nerotypical checks rather than skipping
  them. Worker qualification passed four tests with both checkers on,
  plus 22 feature-disabled rejections. Required verification for checked
  source still runs ordinarily; no analyzed domain was expanded.
- **Archon — existing profile verified in a separate Debug build:** All
  16 focused CTests passed on Linux AMD64 SysV, including 58 native
  differential cases. Stock provisioning failed at the pinned Aurocks
  Perl source needed only for the optional `Translate.py` command.
  Generating the real backend with `--option translate_tool=false`
  produced an installable provider that emitted and ran native code.
  No new native ABI, target or installed-helper qualification is implied.
- **Application — verified for fixed data:** The two-module
  [Energy Budget project](../examples/energy-budget/README.md) reports
  `24380` Wh total, `4260` Wh peak, three over-budget days and `1740` Wh
  summed overruns. The checked-in oracle matched C11, MIR-C11, the project
  VM runner and saved-MIR evaluation. This is a scalar/tuple On1xfile
  example, not a managed, concurrent, metaprogramming or meter-data app.
- **Generated C checks — repaired registration:** `ON1X_TEST_C_FLAGS` now
  reaches the affected CTest registrations instead of being initialized
  after them. Clang 22.1.8 ASan/UBSan runs of `test_ownership_backends.py`
  and `test_mir_decision.py` passed with runtime Cephyr/Archon disabled.
  The flags instrumented the generated C and runtime C compiled by those
  tests, not the GCC-built compiler, VM, or LibStone dependencies.
  The VM backend source test and the regular Archon differential have
  separate registrations and are not covered by this CMake flag fix.
  A separate Clang ASan/UBSan run instrumented `src/backend/mir.cpp`
  and `tests/test_mir.cpp` and passed; linked frontend, scanner and
  Aurocks fault-runtime archives were not instrumented.
- **Deferred:** MIR nominal ownership, move/drop and general loan state;
  `defer` and owner-bearing `Result` cleanup; MIR memory ABI;
  corresponding C11/VM/Archon/analysis support; canonical backend
  capability reporting; wider FFI/pointer translation; substantial
  managed, concurrent and metaprogramming applications; and compiler
  bootstrap work. The existing `on1x.selfhost`
  fixture passed at baseline; it exercises native PPX, not a self-hosted
  compiler. No performance improvement or new native-target claim.

## Baseline and reproduction

Linux x86-64, GCC/G++ 16.2.1 (20260810), Clang 22.1.8, CMake 4.4.3
(Unix Makefiles), Python 3.14.7 and Perl 5.42.2. The MuLIB root project
configures separately; build On1x with `-S on1x`. The qualified baseline
used generated ASDL/Aurocks, MEMGraph with LLSynth, Firestone regex,
Wetstone and the SDK. Runtime Cephyr, Archon, Chisel, Nerotypical and
Novostone were disabled in this baseline. Host Cephyr is independently
required for FFI generation. The first host library tested lacked
`cephyr_emit_mir`; the successful library exposed it under `CEPHYR_1`,
reported version 0.1.0 and had SHA-256
`5de2f63e315381c2b14bf18316e6e3537c242660ada400f0a3ce706e48f7f271`.

```sh
L=/absolute/path/to/pinned/libstone
CEPHYR=/absolute/path/to/compatible/libcephyr.so
B=/absolute/path/to/on1x-build
cmake -S on1x -B "$B" -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON -DON1X_REQUIRE_LIBSTONE=ON \
  -DON1X_LIBSTONE_ROOT="$L" -DON1X_STDFFI_CEPHYR_LIBRARY="$CEPHYR"
cmake --build "$B" -j 8
ctest --test-dir "$B" -j 6 --output-on-failure
ctest --test-dir "$B" --no-tests=error -j 4 --output-on-failure \
  -R '^on1x\.(mir_hir|mir_borrow_hir|mir_decision|mir\.wire_grammar|vm_result|vm_data|vm\.(opcodes|source)|numeric_limits|sdk\.(tool|project_installed_consumer)|stdffi\..*|ownership_backends)$'
```

Unmodified baseline: 238 passed, 1 skipped, 7 failed of 246.
`on1x.container_pod_cgroup` skipped. Failures were `on1x.stdlib_project`,
`on1x.numeric_limits`, `on1x.sdk.tool`, `on1x.sdk.project_installed_consumer`,
`on1x.processor.snapshots`, `on1x.mir.wire_grammar` and `on1x.vm.opcodes`.
Baseline installation `on1x.install` and PPX bootstrap `on1x.selfhost`
passed. Do not use the SDK source-tree tool result as installed SDK evidence.
The first integrated targeted run passed all 11 tests: MIR verifier and
lexical-loan HIR, MIR decision differential, VM data and `Result`,
cross-backend ownership, the SDK tool, and the four FFISpec tests.
The full integrated Release build finished. Its CTest suite finished with
**244 passed, 1 skipped, 2 failed out of 247**. The numeric v3/v4,
MIR wire-grammar, O1VM opcode, SDK tool and relocated SDK consumer tests
that failed at baseline all passed. The same two unchanged failures remain:

- `on1x.stdlib_project`: the test counts undeclared empty
  `stdlib/mcp/__init__.on1x` as a package member. The packager only stages
  declared module inputs. Determine whether the directory is a placeholder
  or a real module; do not widen packaging to arbitrary discovered files.
- `on1x.processor.snapshots`: the checked-in On1xMETA bootstrap parser
  differs from fresh LLSynth output at the pinned dependency (including
  generator/runtime drift and embedded source paths). Resolve reproducible
  snapshot inputs and check bootstrap parity before regenerating it.

The cgroup fixture remained skipped. Neither failure nor the skip was
suppressed or counted as a pass.

For the generated-C sanitizer differential, with a built `B`:

```sh
python3 -B on1x/tests/test_ownership_backends.py \
  "$B/on1xc" "$B/on1x-vm" "$PWD/on1x" "$B/sanitized-ownership" clang \
  --cflag=-fsanitize=address,undefined --cflag=-fno-sanitize-recover=all \
  --cflag=-fno-omit-frame-pointer
python3 -B on1x/tests/test_mir_decision.py \
  "$B/on1xc" "$PWD/on1x" "$B/sanitized-mir-decision" clang \
  --cflag=-fsanitize=address,undefined --cflag=-fno-sanitize-recover=all \
  --cflag=-fno-omit-frame-pointer
clang++ -std=c++20 -O1 -g -Wall -Wextra -Wpedantic -Werror \
  -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer \
  -Ion1x/include on1x/src/backend/mir.cpp on1x/tests/test_mir.cpp \
  "$B/libon1x_frontend.a" "$B/libon1x_scanner.a" \
  "$B/libstone-aurocks/libaurocks_fault_runtime.a" -o "$B/mir-verifier-sanitized"
"$B/mir-verifier-sanitized"
```

## Benchmark smoke

The MuLL benchmark project built with GCC 16.2.1 and passed all nine CTests.
The following used the separately qualified *baseline* native Debug build
(`ARCHON_BUILD`) and GCC `-O2` for emitted programs:

```sh
cmake -S on1x/benchmark -B "$BENCH_BUILD" -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Release
cmake --build "$BENCH_BUILD" --parallel 2
ctest --test-dir "$BENCH_BUILD" --output-on-failure
"$BENCH_BUILD/on1x-benchmark" run \
  --compiler "$ARCHON_BUILD/on1xc" \
  --archon "$ARCHON_BUILD/backends/mir-archon/on1x-archon-mir" \
  --backend cross --optimization both --cases recurrence,branches,tuple,trap \
  --iterations 1500 --seed 1729 --warmups 1 --repeats 3 \
  --compile-warmups 0 --compile-repeats 2 --timeout-ms 30000 \
  --cc /usr/bin/cc --cflags '["-O2"]' --output "$BENCH_OUTPUT"
```

All 12 fixture/backend oracle rows passed, including the untimed ordered
trap. For example, recurrence median wall time was 0.452 ms for C11,
0.446 ms for Archon and 0.479 ms for optimized Archon. Median compile/link
time was 46.0, 34.8 and 38.9 ms respectively, and executables were 51,688,
70,680 and 70,680 bytes. These short, process-inclusive smoke samples do
not establish a performance ranking or measure the consolidation changes.

## Next decision gate

The first candidate ownership increment is a finite, non-generic one-field
nominal box with explicit creation, move and destruction. It is a proposal,
not an admitted format. Define a single canonical MIR representation and
explicit opt-in admission so none of the existing codecs, SDK/VM adapters,
or optimization passes accept the new operations merely because the
verifier learned them. Establish exact nominal and place identities; first
allow at most one block-local live owner with an explicit live-interval
allowlist and no implicit destruction on traps. Validate source lowering
from canonical HIR, evaluator events and normal-exit obligations against
direct C11, plus round-trips and malformed ingress rejection through every
legacy boundary. Do not lower `take` as a scalar value: its source result
is the nominal payload. Broader CFG lifetime, borrowing, aggregate cleanup,
and native execution each require a subsequent gate.

The frontend's representation already canonicalizes labeled layouts:
`src/type.cpp` orders `Type::data` labels and fields together, and
`src/nominal.cpp` retains the resolved nominal identity and generic
arguments. Specify how declaration order and canonical layout order relate
before emitting a new record/sum wire shape. Do not reuse the VM's
identity-erasing projection as evidence of a canonical nominal layout.

For capability reporting, start by inspecting the compiler's existing
linked provider registry, not by adding a parallel table. Publish exact
request envelopes (phase, representation, artifact and target), and keep
inspection, semantic admission, build/link readiness, and independent
execution evidence distinct. The existing JSON and SDK help remain
separate until that migration is implemented.
