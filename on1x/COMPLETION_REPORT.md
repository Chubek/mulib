# On1x completion report

This continuation starts at MuLIB `681ceacf213673ebc1978d71d8cf6821bf35a7fb`.
The final implementation/test revision is `565de4f5`; the follow-up
documentation commit records results without changing compiler behavior.
The initial worktree was clean; the LibStone gitlink and checkout both identify
`3be5e9d7ffde4f97f333365e7080e316396a0e55`. No vendor changes were made.
The recursive audit fails inside that pin on a legacy unmapped gitlink; see
[`M0-VENDOR-RECURSION`](COMPLETION_MATRIX.md).

The root manuals listed by the mandate were removed by `f3bae016`. The
historical copies can be inspected with `git show f3bae016^:on1x/VERIFICATION.md`
(or the other removed filename). The current sources are the
[language reference](docs/language.md), [capability matrix](docs/capability-matrix.md),
[KernelBind manual](docs/kernel-bind.md), [Category manual](docs/category.md),
[backend manual](backends/README.md), [SDK](sdk/README.md),
[runtime SDK](runtime-sdk/README.md), and [standard library](stdlib/README.md).

## Toolchains and configurations

- Host: Linux `6.18.52-1-lts`, x86-64, glibc 2.44; CMake 4.4.3,
  GCC 16.2.1, Clang 22.1.8, Git 2.55.0, Python 3.14.7, Perl 5.42.2.
- Generated scanner: `cmake -S on1x -B build/on1x-baseline -DBUILD_TESTING=ON`.
  CMake reports LibStone ASDL+Aurocks. The enabled `ON1X_` Boolean options
  include extension SDK, dynamic SDK plugins, native SDK reference,
  native PPX, GrammarEmbed, trusted dynamic runtime, compiler-backed addon
  tests, Yellowstone linking, MEMGraph, QMR, Syren, and Wetstone.
  `ON1X_USE_FALLBACK_SCANNER`, Cephyr, Archon, ExoML/Exotract, Chisel,
  Novostone, Firestone HIR, and Nerotypical were off in this configuration.
  The independent Firestone **regex** SDK was available.
- Optional generated-scanner configuration: `-DON1X_REQUIRE_LIBSTONE=ON
  -DON1X_WITH_CEPHYR=ON -DON1X_BUILD_MIR_ARCHON=ON
  -DON1X_ARCHON_ENABLE_SDK_ADAPTER=ON
  -DON1X_ARCHON_TARGET_TRIPLE=x86_64-unknown-linux-gnu
  -DON1X_WITH_EXOML=ON -DON1X_WITH_EXOML_EXTRACT=ON
  -DON1X_WITH_EXOTRACT=ON`. Set `Cephyr_DIR` and `ArchonBackend_DIR` to
  installed CMake packages. This run used
  `-DCephyr_DIR="$PWD/build/sync-prefix/lib/cmake/Cephyr"` and
  `-DArchonBackend_DIR:PATH=/mnt/andromeda/tempdir/opencode/on1x-archon-toolchain/install/lib/cmake/ArchonBackend`.
  The subprocess-based native driver currently expects the explicit Archon
  cache entry to have CMake type `PATH`; an untyped entry let CMake configure
  but failed in the native build test. Exotract's Lua frontend was not enabled.

## Reviewed changes

- `65c56197` fixes the M0 editor assertion that selected the embedded `1` in
  `on1xfile` instead of the manifest version token. It independently asserts
  directive and version highlighting.
- `d2dc4e86` updates the read-only knowledge index to retain current capability
  and verification documents in scope along with historical paths, and removes
  a hard-coded temporary directory from its tests.
- `d0f62325` keeps the Pod fixture's deliberate live-thread rejection separate
  from its single-threaded cgroup assertions; Linux can keep the just-joined
  thread visible in procfs briefly. The fixture passed eight consecutive runs.
- `d1c3b58c` restores ordinary contextual `match` calls before blocks without
  changing valid legacy arms; both scanners have focused evidence.
- `92e3a3ee` checks explicit generic binders at the recursive-component
  boundary, preventing declaration order from changing generic validity.
- `733ed116` covers labelled exits from nested lexical ownership scopes,
  using runtime allocation counters and a rejection on a repeatable edge.
- `6e4a0bd6` strengthens generated C11 scalar adapter checks against the
  extracted integer type. No callable or safe admission is broadened.
- `3319922e` preserves parent descriptor state during captured process setup,
  including the seven combinations with closed standard descriptors.
- `a02c0c4b` makes checked-HIR specialization short-circuit without moving or
  deleting strict arguments; tests compare C11 and MIR evaluation.
- `ad800327` accounts for definite-assignment set work in the MIR verifier,
  and independently checks rejection of a partially initialized join.
- `15c3c906` makes real VM bytecode execution report the rejected callee on
  frame budget exhaustion, matching the MIR evaluator in the exercised case.
- `70c33843` fixes whole-atom postfix quantifiers for quoted GrammarEmbed
  token literals.
- `7d4b26a8` records every published source-library file digest in the
  existing report schema, with a relocated downstream execution test.
- `470d44ac` adds ordered compatibility `ListPair.map` and `map_eq` without
  asserting full SML Basis parity.
- `8c42decc` fixes byte-corpus insertion in the optional FFI string fixture.
- `c02cafb7` and `565de4f5` add and register the nested slice snapshot
  differential across direct C11 and saved VM artifacts.

## Verification

The generated GCC integration at `70c33843` ran all 215 scheduled entries:
**214 passed, 0 failed, 1 skipped** (`build/on1x-integration-ctest.log`).
The editor and MCP tests passed, and the standalone MCP suite passed **40/40**.
The final default run uses `cmake --build build/on1x-baseline --parallel 4`
and `ctest --test-dir build/on1x-baseline --output-on-failure --parallel 4`.
At `565de4f5`: **215 passed, 0 failed, 1 skipped** of 216 scheduled.
Logs: `build/on1x-final-build.log` and `build/on1x-final-ctest.log`.

Independent Clang fallback baseline on the unmodified implementation:
**210 passed, 3 failed, 1 skipped**. Reproduce in a separate checkout with
`-DBUILD_TESTING=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
-DON1X_USE_FALLBACK_SCANNER=ON -DON1X_LIBSTONE_ROOT=/path/to/pinned/libstone`.
The two editor failures were corrected. `on1x.processor.snapshots` failed
because LLSynth embeds checkout-specific absolute paths in the generated
parser's `file` and `dependencies[].path` metadata even when the grammar
hash agrees. Reproduce with `ctest -R '^on1x\.processor\.snapshots$'` from a
build in another checkout. A follow-up must canonicalize those fields in LLSynth
or in `hir/processor/snapshot.py` before `provenance()` hashes the generated
source, and add a two-root generation comparison;
do not ignore semantic differences or replace the generated parser.
Clang also reports an existing missing `Borrow` case in the SDK frontend
AST tag mapper. Baseline log:
`/mnt/nvme/on1x-workers-681ceacf/12/build/worker12-logs/ctest-nvme-tmp.log`.
The final Clang/fallback rerun at `565de4f5` finished with **213 passed,
1 failed, 1 skipped** of 215 scheduled. Both editor tests, the ownership
regressions, and the slice differential passed. The only failure was the
same snapshot relocation issue; the same cgroup test was skipped. Final log:
`/mnt/nvme/on1x-workers-681ceacf/12/build/worker12-logs/final-ctest.log`.

Optional GCC generated run: **6/6** Cephyr and ExoML contract tests passed
(`build/on1x-optional-ffi-cephyr.log`). The Chisel-enabled compiler from the
FFI worker and the integrated binder passed the separate string fixture with
its repaired byte corpus. The optional Archon selection then passed **9/9**
on Linux AMD64, including native compile/link/execute, scalar ABI generation,
and differential/limit tests (`build/on1x-optional-archon-ctest.log`).
These are finite executable profiles, not certification for other architectures
or new safe FFI profiles.

Focused Clang ASan/UBSan ran the emitted C11/runtime for recursive ownership,
closed process descriptors, slice snapshots and ListPair. These tests do not
instrument the entire compiler or VM and are not a whole-suite sanitizer run.
The final ownership and slice checks can be rerun with:

```sh
python3 -B on1x/tests/test_recursive_owning_adts.py "$PWD/build/on1x-baseline/on1xc" \
  on1x build/on1x-owner-final-san clang --cflag=-fsanitize=address,undefined
python3 -B on1x/tests/test_acceptance_slice_snapshots.py "$PWD/build/on1x-baseline/on1xc" \
  on1x build/on1x-slice-final-san clang --cflag=-fsanitize=address,undefined
```

The initial generated profile scheduled 215 entries; CTest reported 211
passes, two editor assertion failures, one intermittent Pod assertion failure,
and one cgroup skip. The Pod fixture requires an exclusive delegated `cgroup v2`
parent via `ON1X_TEST_POD_CGROUP_PARENT` for that optional test. A parallel
follow-up attempt overlapped a rebuild and returned executable permission errors;
those results are not used as regression evidence. The preferred temporary
volume also filled, so later logs are retained under `build/` rather than in
the source directories.

## Migration and open limits

No schema migration is needed. Explicit generics that a recursive sibling
previously specialized accidentally now reject. Regenerating C11 FFI adapters
adds the primitive assertion; a same-layout but distinct typedef must be
re-extracted and reviewed, not waived. Consumers of source-library build
reports can now verify entries under `outputs["library/..."]`.

The unqualified features listed in the [completion matrix](COMPLETION_MATRIX.md)
remain open. In particular, this does not implement general safe FFI, effectful
ownership MIR, stackless async, broad target equivalence or full SML Basis
conformance, and does not complete the release milestones.

## Ownership and stackless frame continuation

This continuation starts at `825f6585` with the same pinned LibStone,
GCC 16.2.1 and Clang 22.1.8. It adds the [checked MEMGraph and MIR
scalar lexical-slot profile](docs/ownership-backends.md), SDK mode
inspection and an unchanged-ABI On1xRT ownership integration fixture.
It also adds a [stackless timer-frame profile](docs/async-frames.md) on the
existing taskgroup scheduler. Its `launch async` and `await timer` forms
lower through checked HIR to direct C11, not to fibers. No LibStone sources
or submodule revisions changed.

- Optional generated build: **2/2** `ownership_backends` and
  `mir_borrow_hir`, including real AMD64 Archon and Cephyr products
  (`build/on1x-ownership-optional-ctest.log`).
- Clang forced fallback and `ON1X_WITH_MEMGRAPH=OFF`: **4/4** ownership
  projection, backend, HIR and embedding tests
  (`build/on1x-loans-fallback-final-ctest.log`).
- Direct differential passed with Clang ASan/UBSan on generated C11 and
  runtime; the emitted VM and native code were not sanitizer-instrumented.
  The On1xRT ownership fixture and private async-frame C runtime separately
  passed strict Clang ASan/UBSan with leak detection.
- **6/6** integrated concurrency and installation tests
  (`build/on1x-async-csp-integration.log`). The runtime fixture checks that
  each frame suspends without allocating a per-frame fiber stack.
- Generated-scanner GCC integration scheduled 220 tests:
  **218 passed, 1 failed, 1 skipped** (`build/on1x-ownership-verified-ctest.log`).
  The sole failure was an indentation error in the new PPX loan-boundary
  assertion. Commit `a0686063` corrected the test and its full PPX suite
  passed on rerun (`build/on1x-ownership-ppx.log`). The cgroup test still
  requires a delegated parent.
- At `9d84654b`, the source timer block adds typed HIR slots, checked resume
  targets, and C11 frame/resume emission. The independent fallback HIR test
  and generated-frame Clang ASan/UBSan run passed; the C probe measures no
  extra stack and exactly-once frame free. The optional Cephyr rejection and
  existing ownership differential passed in `build/on1x-ownership-async-cephyr.log`.

The scoped-loan profile uses existing MIR wire versions and ordinary
slot initialization checks. SDK borrow flags are additive inspection
metadata; older HIR readers can reject unfamiliar flags. General managed
MIR operations, public runtime ownership transport, and async functions
with task/channel readiness or resource-bearing suspension remain open.

The [finite snapshot iteration profile](docs/finite-iteration.md) uses
existing checked labelled loops and latches (`8d124ef6`, `0448b733`).
Integrated fallback and optional configurations passed their 5-test
selections (`build/on1x-iteration-fallback.log`,
`build/on1x-iteration-optional.log`). They cover direct C11, plain and
optimized VM artifacts, and Cephyr AMD64 for local slices. The separate
Archon driver regression (`c2d2ccce`, `build/on1x-finite-archon-gate.log`)
rejects the aggregate layout without replacing the existing output.
Typed lists, cons/tail patterns, and general iterators remain outside
the profile.

The final combined GCC/generated-scanner run at `0448b733` passed **219
tests, 0 failed, 1 skipped** of 220 scheduled, including the new ownership,
iterator and stackless timer tests. The skip is the delegated cgroup fixture.
Logs: `build/on1x-integrated-ownership-iter-async-build.log` and
`build/on1x-integrated-ownership-iter-async-ctest.log`.

## ArchVirt and On1xfile auxiliary continuation

Started at MuLIB `f724fed93f425ad525709b3651bf0a85b3490c7b` with LibStone
`21f886afe61ce27ef5684acde8f1ed7c064e4e6a`. The pin and its sources were
unchanged. The top-level gitlink matched; recursive submodule audit still
encounters LibStone's unmapped `.decomissioned/lapislazuli/.info/binutils`.
Test host: Linux x86-64, KVM API 12, GCC 16.2.1, Clang/LLD 22.1.8, CMake
4.4.3 and Python 3.14.7.

- `99dd9c3b`, `08237fb1`, `661d9aa8`, `4491083a`: checked source-to-DomIOS
  ELF, native KVM launch, runtime-archive fingerprint, exact-image replay
  and linked activation policies. `readelf -d` confirmed `libdomvirt.so` is
  a dependency of the emitted launcher. A separate device-namespace probe
  with no `/dev/kvm` returned 125 with a KVM diagnostic and no guest output.
- `0ae44245`, `404cea53`, `4491083a`: root-only project policy, frozen
  linked build, separate build sandbox and VM launch, exact cache commands,
  and logical source paths across staging directories.
- `aa1af7e5`: `std.vmm` config and bounded trusted C companion. The two
  profiles do not admit direct safe On1x VM management. `59e0a969` kept the
  companion out of the pure source package and added SDK tool validation
  for the root-policy lockfile while retaining v1 and v2.
- `9ce6ec67`–`12d9b247`, `4491083a`: `on1xc aux` and explicit recursive and
  template-aware `aux init`, including the installed project tool.

Configuration and verification:

```sh
cmake -S on1x/third_party/libstone/cornerstone/ArchVirt \
  -B build/on1x-archvirt-upstream -DCMAKE_BUILD_TYPE=Release \
  -DDOMVIRT_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX="$PWD/build/on1x-archvirt-prefix"
cmake --build build/on1x-archvirt-upstream --parallel 6
cmake --install build/on1x-archvirt-upstream
cmake -S on1x -B build/on1x-archvirt-enabled -DBUILD_TESTING=ON \
  -DON1X_WITH_ARCHVIRT=ON -DON1X_ARCHVIRT_ROOT="$PWD/build/on1x-archvirt-prefix"
cmake --build build/on1x-archvirt-enabled --parallel 5
ctest --test-dir build/on1x-archvirt-enabled --output-on-failure --parallel 4
```

The pinned upstream C API smoke booted its bundled software guest on all
three machines. GCC/generated focused integration passed 11/11 during
integration (`build/on1x-archvirt-project-matrix.log`); Clang forced fallback
with MEMGraph, SDK, Syren and Wetstone disabled passed 7/7
(`build/on1x-archvirt-fallback-ctest.log`). The unchanged upstream library was
not instrumented; the strict Clang ASan/UBSan host runner passed the real
KVM suite, including arithmetic traps, bounded serial, wrong ELF and a
watchdog. The staged installation passed ArchVirt CLI, `aux`, and `aux init`
fixtures.

The enabled full run after the packaging/SDK fixes scheduled 227 CTests:
225 passed, the delegated-cgroup fixture skipped, and the existing
cancellation/deadline fixture failed once when the Python C11 build driver
exited with `-11` after reporting a `manifest_dependencies` error. Its
isolated rerun passed. Both outcomes are recorded in
`build/on1x-archvirt-enabled-verified-ctest.log` and
`build/on1x-archvirt-cancellation-rerun.log`.

Existing C11, VMWeave and enabled native commands are separate from the
[ArchVirt guest profile](docs/archvirt.md). Existing v1/v2 locks still work
unless the root project opts in; a VM project must re-lock after changing
its root VM policy. Native host architectures other than x86-64, general
host-runtime features in the guest, and safe On1x VMM lifecycle admission
remain open. See [VMM admission requirements](stdlib/vmm/README.md#admission-and-remaining-work)
for the precise missing ownership, buffer, and native-effect contracts.

### Native Archon timer continuation

`1d70ce4f` adds the standalone `on1x-archon-mir --timer-source` path
documented in [TIMERS.md](backends/mir-archon/TIMERS.md). It uses checked
timer HIR, verified scalar MIR, real generated AMD64 code and the same CSP
supervisor as the C11 path. Lead integration **11/11** passed against the
generated backend from the same LibStone pin
(`build/on1x-archon-timer-integrated-ctest.log`). Worker runs with the SDK
adapter and fallback scanner passed independently; only the runtime and
ABI shim, not the emitted native assembly, were sanitizer-instrumented.
No public MIR suspension or SDK execution contract is added.

### Live ArchVirt sessions

`69006df7` assigns content IDs to emitted guest and linked images.
`cf9cd971`, `f6f362c2`, and `6b7e8697` add the interactive serial protocol,
the owner-only session supervisor and the MuLIB line editor. The installed
and build-tree tests compile a real guest, seed it in detached mode,
clear the replay cache, send a second parameter to the same VM, attach the
console, and confirm the accumulated state. Project preparation
(`on1xc aux run --prepare`) registers an ID without launching.
The combined focused GCC selection passed 14/14
(`build/on1x-vmm-enabled-verified-ctest.log`). The final fallback selection
covered CLI, actual KVM and host session transport (3/3,
`build/on1x-vmm-fallback-final-ctest.log`). A Clang ASan/UBSan rerun of the
host fixture found that an error reply could race endpoint cleanup; moving
the reply after cleanup fixed it in the integration follow-up. The worker
also tested the real 60-second guest idle timeout; the 300-second wall
deadline was not exercised end-to-end.

The full ArchVirt-enabled GCC/generated-scanner run with live sessions
passed **228 tests, 0 failed, 1 skipped** of 229 scheduled
(`build/on1x-archvirt-combined-ctest.log`). The skip was the delegated
cgroup fixture. After the final startup cleanup ordering fix, both session
suites passed four consecutive Clang fallback runs
(`build/on1x-vmm-fallback-readiness-ctest.log`).

### MCP session and project initialization continuation

At baseline `68f5f143` and unchanged LibStone `21f886af`, `232f5488` and
`70e516ea` add the private stdio MCP profile, managed ArchVirt lifecycle and
MuLIB public-header inventory/inquiry. The default HTTP deployment stays
read-only. The VM tests use real KVM and bubblewrap, with a temporary override
of the production runtime directory because the test account does not own
`/var/run`. `f42351a7` adds `aux init --with-gitignore`, preserving existing
project files and writing the supplied template only on request.

The independent Clang fallback selection passed 4/4
(`build/on1x-mcp-fallback-ctest.log`). The 41-case knowledge suite passed,
including all 2,053 public MuLIB headers in the live index and refusal to
invoke a VM tool through HTTP. After 61 seconds of parameter idle time, a
real MCP-owned guest resumed and correctly used its earlier state
(`build/on1x-mcp-vmm-long-idle.log`). A busy guest still hits the active
deadline. EOF, SIGTERM, isolated IDs, explicit delete/invalidate, and
SIGKILL's kernel parent-death behavior are covered; SIGKILL can leave files
for operator cleanup. The staged installation also passed the KVM MCP and
source/installed init fixtures (`build/on1x-mcp-install.log`). No LibStone
revision or SDK ABI changed.

The full ArchVirt-enabled GCC/generated-scanner integration scheduled 230
CTests: **229 passed, 0 failed, 1 skipped**. The skip was the
delegated-cgroup fixture. See `build/on1x-mcp-combined-ctest.log`.
