# On1x release qualification and evidence

**Date:** 2026-11-26. **Release disposition:** not release-ready.

**Inputs:** MuLIB `26c4fc0845be2ffd120ce0d68b4b7c5ee7c95be9` plus the
qualification changes; bundled LibStone
`76d24a7f8b19294bab2b713dc69f799e412b62d2` (read-only).
The retired LibStone binutils gitlink has no `.gitmodules` mapping and is
outside the On1x build. Build On1x with `-S on1x`, not the MuLIB root.

`B` below denotes the run directory
`/mnt/nvme/mulib/build/on1x-release-26c4fc08.dwrbaW`. Paths below are
run-local evidence, not checked-in binaries or portable prerequisites.
The starting revision, LibStone tree entries and host tools are also
recorded in `$B/dependency-identities.txt`.
The principal records combined runs separately from focused worker tests.
Use the [capability matrix](capability-matrix.md) for source status and the
[verification index](../VERIFICATION.md) for archival runs.

## Release checklist

| Gate | Required evidence | Disposition |
| --- | --- | --- |
| Inputs and host provisioning | Exact dependency, tool and configuration identity; record host Cephyr and Archon provenance separately from bundled source, and do not treat generated target resources as executed tests. | Partial. Fresh Cephyr passed focused checks and a full FFISpec scan on retry with a historical QLD bootstrap; combined suites still used the earlier host package. |
| Generated scanner | Actual generated scanner compile, scanner parity with independent fallback, combined CTest inventory and failure/skip accounting. Missing prerequisites must not select fallback implicitly. | **VERIFIED** for the focused compiled scanner/parity selection. The relinked full Release run still had one failure (passed on rerun) and one skip. |
| Forced fallback | Build and test the independent bootstrap scanner profile, with build-host FFI prerequisites recorded and optional features identified. | Full Release run: one process failure, passed on rerun; one cgroup skip. The focused scanner selection was **VERIFIED**. |
| Restricted stdlib packaging (W1) | Explain `mcp/__init__.on1x` omission and assert the intended declaration-only staging and downstream behavior. | **VERIFIED** for the revised focused suite on the fresh fallback compiler; old test mismatch reproduced. |
| On1xMETA bootstrap snapshots (W2) | Compare pinned-generator and bootstrap content, deterministic relocation and stale or malformed input checks. Keep distinct from frontend scanner generation. | **VERIFIED** for production processor artifacts, relocation and the Perl-free processor probe; not a no-Perl compiler build. |
| Linked backend report (W3) | Actual registrar/descriptor envelope, scope and absence semantics, configured and optional provider discrimination, and negative invocation cases. | **VERIFIED** for the linked report in the first generated integration and W3's pre-owner AMD64 selection; no execution claim from report contents. |
| Explicit nominal-owner MIR (W4/W5) | Named verifier, evaluator and C emitter; positive lifetime/result checks, malformed owner mutation and legacy ingress rejection on a coherent rebuild. | **VERIFIED** for the narrow in-memory evaluator/C11 oracle and tested legacy rejection paths. First generated owner CTest passed; this is not SDK owner execution. |
| Real non-clamp kernel fixture (W6) | Discovery, exact ABI and independent authorization, link and execution, independent result oracle and negatives, with excluded backends and resource shapes. | **VERIFIED** for reviewed static PriceOpt candidate selection in the named manual runs and in the generated compatibility CTest build with Exotract/Lua. |
| Installed and relocated consumers (W7/coordinator) | Install from a named configuration, move the prefix, exercise CLI/SDK/project consumers outside source/build paths, and test optional backend artifacts if claimed. | **VERIFIED** for fallback and generated SDK/project/Mune/QMR, plus generated moved-prefix Archon and VM checks. No general optional-runtime relocation claim. |
| Final release accounting | Record exact configure/build/test commands; outcomes by configuration and optional provider; include failures, skips, not-run and unregistered gates. Keep smoke and sanitizer coverage scoped to instrumented sources/products. | Blocked on repeatable release-wide results and unqualified profiles. See combined runs and remaining gates below. |

## Earlier evidence — not from this task

- The [first consolidation increment](consolidation-status.md#historical-baseline-and-reproduction)
  ran its unmodified baseline at **238 passed, 7 failed, 1 skipped of
  246** and the integrated Release configuration at **244 passed,
  2 failed, 1 skipped of 247**. The two remaining failures were
  `on1x.stdlib_project` and `on1x.processor.snapshots`; live cgroup enforcement
  was skipped. This was based on MuLIB `167286b8…` with LibStone `ac951609…`,
  not the revisions above.
- The [load-time clamp qualification](kernel-load-qualification.md#regression-and-sanitizer-commands)
  separately reproduced those failures with a fallback-scanner C11
  configuration. Archived optional checker/Archon, sanitizer and benchmark
  results in the earlier reports do not qualify this task's profiles.
- The [backend phase audit](backend-phase-audit.md#verified-slice) documents
  historical evidence for fixed scalar and copy-tuple loan fixtures, including
  source and saved-artifact routing. It does
  not establish full construct coverage, nor current optional provider or
  installation evidence.

## This task — baseline and host provisioning

| Observation | Scoped result and evidence |
| --- | --- |
| Initial generated configure, GCC/G++ 16.2.1, Unix Makefiles, Release | Bundled LibStone/Aurocks scanner recognized; configure stopped at `hir/FFIHandler/CMakeLists.txt:34` for missing build-host Cephyr (`$B/generated-config.log`). This is not an On1xMETA stale-snapshot outcome. |
| Build-host Cephyr | Still required for FFI recipe generation when forcing scanner fallback or disabling optional in-process Cephyr. Accepted sources are `ON1X_STDFFI_CEPHYR_LIBRARY`, `CEPHYR_LIBRARY`, or `Cephyr_DIR`. The provisional historical `build/kernel-load-cephyr-prefix` library exports `cephyr_emit_mir@@CEPHYR_1`; its `libcephyr.so.0.1.0` matches the historical build-tree copy at SHA-256 `b01c7b106e10944938ef0a90ba44676fa296b7dfe7312cf6e9ec7bef1dcbb987` (W1). A clean On1x build using it is not a pristine pinned dependency build. |
| Fresh pinned Cephyr and Archon attempts | Standalone Cephyr's expected top-level valijson path differs from its tracked nested path; a later attempt reached a separate QLD dependency on the deleted `LibStone/scripts/aurocks.pl`. Standalone Archon provisioning also reached a deleted `cornerstone/Aurocks/scripts/aurocks.pl` dependency in `irgen/translate_tool.py`. Do not infer optional native execution from these attempts. See `$B/cephyr-{pinned-config,host-config-clang-runtime,host-build}.log`, `$B/archon-provision.log` and W7's handoff. |
| Rebuilt Cephyr (historical parser prerequisite) | A fresh GCC build from the separate `/mnt/nvme/libstone` source tree used the historical QLD Perl parser (`7878cb91…`) and fresh target CRTs. It installed to `$B/cephyr-host-prefix`; `libcephyr.so.0.1.0` SHA-256 `7e2519ddc78980025f4e4e54c2a2c61c7ecff9113c401d2bd883781659726801`. Focused FFISpec passed **15/15**, and the separate Clamp ABI controls passed (`$B/cephyr-host-{build-legacy-override,install,ffispec,clamp-compat}.log`). Full-kernel FFISpec generation segfaulted in the first attempt (`$B/generated-ffispec-final.log`) and completed in a separate fresh output directory. Broad release retains the historical host library (`$B/generated-ffispec-release.log`). No pristine host, general foreign coverage or replacement-library sign-off. |
| Optional Nerotypical/Satie configuration | `prepare-type-sdk.py --output "$B/satie-euf"` initially returned success without `SATIE_EUF_ONLY`: Git skipped applying the root-relative patch from within the parent worktree (`$B/generated-config-broad.log`). After the local Satie projection and Aurocks host forwarding were repaired, the broad generated configuration completed (`$B/generated-config-host-forward.log`). Configuration is not evidence of any checker run. |
| Broad generated build attempt | Build stopped on the missing ExoML parser prerequisite `LibStone/scripts/aurocks.pl`; separately, the initial host FFISpec generator subprocess segfaulted (`$B/generated-build-broad.log`). The serial retry and an ExoML/Exotract-disabled configuration are separate runs. This does not establish the cause of the FFISpec crash. |
| Optional native provider artifacts | Initial historical Archon candidate: LibStone `5d733d69f19fac1d8a05294fe54e4a4ec863fc4c`, archive SHA-256 `1a756d1e1d3b04f66523ac9091f805fcdcc857d98ada88b8342164d6b0e2a498`. The first combined generated integration instead used a projected Archon package with the historical generator for `Translate.py`; its native AMD64 archive is byte-identical to W7's exact-pin backend-only build (no `Translate.py` or Oracle IPC; SHA-256 `bfe88ccfe1d139cada2ac1842a382b3c98038e5b4fd8a4775b2c468dafb548b1`). W7 independently assembled and ran a scalar fixture with the backend-only build (`$B/w7/archon-native-provision.log`). ArchVirt used a separate pre-existing `build/on1x-archvirt-prefix`, shared library SHA-256 `827ea0c73787d9f5072c6e6ba8c5414fd9fc43ac77ec91d79ade71e0d21e825d`; source revision not established. |
| Target runtime inputs | Pinned Cephyr `stdlib/build.sh` emitted AMD64, AArch64 and RV64IM CRT/libc using Clang 22.1.8 and llvm-ar; two builds from different paths matched all six outputs. ELF headers identify those architectures; these are target inputs, not native or guest execution evidence. Logs `$B/w7/runtime-clang-{AMD64,AArch64,RISCV64IM}.log`. |
| Fresh pinned native Aurocks | All-GCC Release reached internal compiler errors in C++ IRA at `-O3`; all-Clang reached a Chomsky3 C warning treated as an error. GCC 16.2.1 C plus Clang++ 22.1.8 C++ succeeded with CMake 4.4.3/Unix Makefiles/Release from bundled source. `$B/w7/aurocks-native/aurocks` SHA-256 `260a6debd4d1d2a8c0ef30292d8ffaa9959aab9e3a777ebf7c8d609c654b82da`, Aurocks tree `720dacd55f8fa8334ad9b9c5534c7811fa2e03db`; logs `$B/w7/aurocks-native-{config,build}.log`. Two runs of `python3 -IB on1x/cmake/GenerateAurocks.py "$A" on1x/grammar/On1x.g "$output"` produced four byte-identical files at separate output paths. Standalone Aurocks CTest passed **5/5** (`$B/w7/aurocks-native-ctest.log`). This is host generator evidence, not compiler scanner parity; native-emitted QLD parser still lacks the legacy GLR interface (`$B/w7/ldscript-native-compile.log`). |
| W1 focused reproduction with a historical binary | Unchanged `test_whole_stdlib_and_downstream` using `build/kernel-load-on1x/on1xc` (fallback Release) and `/usr/bin/cc` differed only on the expected-but-unstaged `mcp/__init__.on1x`. Both MCP files were added empty and there is no `On1xfile.m` module declaration in that directory. `$B/w1/prior-compiler-baseline.log`; distinguish from the principal's clean fallback baseline. |
| W2 pinned standalone recorder | `snapshot.py --check` with pinned LLSynth, Perl 5.42.2 and Python 3.14.7 rejected checked-in `generated/On1xMETA.py` (`$B/w2/snapshot-baseline.log`). Old snapshot differs in generator/runtime behavior as well as absolute provenance. Separate source locations change `SPEC.file` and resolved dependency path in raw pinned output; this alone does not prove semantic drift. |
| W2 stale snapshot, current fallback configuration | `ctest --test-dir "$B/fallback" --no-tests=error -R '^on1x\.processor\.snapshots$' --output-on-failure`: **0/1**, stale `On1xMETA.py` (`$B/w2/ctest-snapshot-baseline.log`). This build had Perl and uses fresh LLSynth despite forcing the frontend scanner fallback. Before refresh, old bootstrap and raw pinned fresh parser matched 7 full accepted trees, 5 diagnostics and the exact authored table payload digest (`$B/w2/bootstrap-baseline-parity.log`). This does not establish byte parity. |

See the [dependency provisioning guide](dependency-provisioning.md) for
exact setup recipes and remaining dependency blockers.
Separate negative configurations rejected two mismatched host Aurocks
paths and an explicitly missing Archon backend manifest, respectively
(`$B/negative-host-aurocks.log`, `$B/negative-archon-manifest.log`).

### First integration context — not the final release matrix

The pre-integration fallback run preceded the owner `Module` changes. Its
two fixture failures were the conflicting Category CLI modes and stale
MuLIB catalog assertions; the corrected cases passed **2/2**, without
relaxing admission or expanding the catalog
(`$B/fallback-{ctest-preintegration,corrected-fixtures}.log`).

The first broad generated integration kept Chisel, Nerotypical, Novostone,
Firestone HIR, Syren, Wetstone, Archon, ArchVirt and Cephyr enabled, but
disabled ExoML, extraction and Exotract's Lua path after the generator
prerequisite failure. The initial run passed the named owner/profile and
linked capability tests, and the Archon scalar ABI/differential tests.
It also exposed legacy MIR v1 fixture mismatches and Archon manifest and
installed-runtime defects (`$B/generated-ctest-integrated.log`).
Corrections and a final combined matrix are separate; the initial passes
do not establish clean generated install or relocation. Wider Clang
ASan/UBSan configure/build attempts hit compiler crashes in non-owner C++
sources (`$B/sanitized-build.log`, `$B/sanitized-core-build.log`); sanitizer
claims below stay with the actual focused instrumentation.

## This task — focused profiles

### Restricted stdlib staging

The fix is test-only. The package comparison excludes only the undeclared
empty MCP placeholder; it still asserts exact inventory, omission from
`.on1x-build.json`, and a real failure to import `std.mcp` from the relocated
package. The raw source-tree discovery contract and package staging rules
are unchanged.

The principal used the freshly built `$B/fallback/on1xc` (GCC 16.2.1
Release, forced fallback scanner, SDK and native PPX on, pinned LLSynth
and the historical build-host Cephyr above). The immutable pre-change test
passed **7/8** cases, failing only the MCP mismatch
(`$B/fallback-stdlib-old-test.log`). The updated suite passed **8/8**
cases, reported by CTest as **1/1** registration passing
(`$B/fallback-baseline-stdlib_project.log`, despite the filename).
These are focused comparisons, not the combined fallback results.

The earlier run of the updated
`python3 -B on1x/tests/test_stdlib_project.py --on1xc build/kernel-load-on1x/on1xc --cc /usr/bin/cc`
passed **8/8** with the historical fallback Release compiler, including
packaged parsec initialization and downstream execution. See
`$B/w1/prior-compiler-current.log`; this is not the fresh compiler result.

### On1xMETA provenance and snapshots

Pinned LLSynth tree: `96b4f2522146d13cfb29652338c4e382e79f51f4`.
The repair is scoped to generator reproducibility, bootstrap parity and
tracked snapshot content. Two raw LLSynth paths reflect their input
locations, so the generator wrapper must avoid carrying machine-local
provenance into checked-in content without removing real grammar/dependency
identity or masking semantic differences. This generator (Perl plus
Python) is independent of the native Aurocks source scanner.

The production generator now validates the grammar path and sole dependency
digest, then normalizes only the two embedded provenance paths. Fresh and
bootstrap builds consume identical parser bytes; authored grammar and rule
payload are unchanged. The parser SHA-256 is
`90887d22b64c56dd62e7e0cdc7a2979ad4accc7385051220bb176c23503391e5`.
Regeneration/check and a relocation of both On1xMETA and the exact LibStone
generator reproduced all four checked-in snapshots
(`$B/w2/snapshot-{refresh,full-relocation}.log`).

W2's GCC 16.2.1 / CMake 4.4.3 Release builds used bundled LibStone,
historical host Cephyr, Perl 5.42.2 and Python 3.14.7. The generated build
required LibStone and W7's fresh native Aurocks; the other forced scanner
fallback. Both built `on1x_meta_tables` and `on1x_meta_check` and matched
tracked parser/header/manifest output (`$B/w2/{generated,fallback}-meta-build.log`).
Focused processor generator, snapshots and include-precedence tests
plus compiled scanner cases passed **5/5 generated** (including scanner
parity) and **4/4 fallback** (`$B/w2/{generated,fallback}-final-ctest.log`).
The compiled processor probe passed with configured Perl and with
`CMAKE_DISABLE_FIND_PACKAGE_Perl=ON` from the actual checkout, with identical
output artifacts (`$B/w2/cmake-parity-final.log`). This does not certify a
whole no-Perl compiler: that configuration stops at FFIHandler's Perl
requirement (`$B/w2/bootstrap-config.log`).

### Linked backend capability reporting

The W3 scope is linked SDK built-ins, shaped from the actual registrar map
and descriptors: checked HIR or verified MIR, representation/schema, artifact
and target envelope. `on1xc --backend-capabilities` is stand-alone and
requires `ON1X_BUILD_EXTENSION_SDK=ON`. Keep link discovery, admission
prerequisites, external build/link tool probes and execution evidence distinct. The direct CLI/VM
route is not a linked SDK registrar; a disabled SDK is not an empty list of
available direct compiler routes. This query does not load external plugins,
invoke provider callbacks or collect test evidence. See the
[schema contract](../backends/BuiltinCapabilities.md). The Aurocks scanner
generator and optional Yellowstone registrar-map generator are independent
axes; success for one is not evidence for the other. With a historical
Yellowstone host executable of unproven source revision, W3 generated the
current registrar map and passed the byte-parity, ordering and malformed
manifest checks (`$B/w3/manifest-trial-retry.log`). The historical generator
trial did not execute a linked compiler query.

W3's pre-owner report checker passed portable Archon-off (five providers),
generated-map AMD64 (six), and a generated-map mismatched-target case that
kept Archon linked but reported its prerequisite unsupported
(`$B/w3/preowner-relink.log`). W3 subsequently linked the pre-owner Linux
AMD64 SDK compiler and generated
registrar map against W7's exact-pin backend-only Archon package. The
six-provider report matched the focused checker. In a **separate** native
call, trusted SDK assembly emitted for `x86_64-unknown-linux-gnu` linked
with the scalar runtime, exited 0 and printed `42`
(`$B/w3/preowner-pinned-archon-test.log`). The mismatched AArch64 request
earlier rejected without an artifact (`$B/w3/preowner-native-smoke.log`).
The report itself never performs those operations. The named
`on1x.backends.capabilities` test also passed the first generated combined
run (`$B/generated-ctest-integrated.log`), not a claim for final installed
binaries or other targets.

### Nominal-owner MIR

[The W4/W5 contract](mir-nominal-owner.md) defines the first in-memory
profile: exact module-local nominal declaration identity, one labeled `i64`
field, disjoint owner places, ordered create/move/drop, at most one live
owner, explicit drop before normal return and no ordinary operation inside
the live interval. Scalar output outside the interval is not observation of
an owner's payload; the lifecycle test compares host-only create/move/drop
tracing of declaration/place IDs and retained payload against an independent
C11 fixture. Local storage
does not imply real allocation, guest cleanup on trap or an owner ABI.
The first named opt-ins do not widen source lowering, MIR v1–v4 checks,
text/CFG, SDK, VM, Archon, passes, or FFI. The new internal `Module` layout
requires rebuilding dependent objects together; no evidence from mixing
pre-change archives with new objects is accepted here.

W4's standalone current-source verifier probe passed **5 positive and
60 negative** cases under GCC 16.2.1 and Clang 22.1.8 ASan/UBSan, with a
legacy scalar v1 control and a work-budget boundary check.
It links only `mir.cpp`, `type.cpp` and `primitives.cpp`.
See `$B/w4/probe-{gcc,clang-san}.log`. This is verifier-only, not evaluator,
emitter, SDK, pass, VM, Archon or installed-product evidence.

W5's named evaluator, strict emitted C11 and handwritten C11 agreed on
`42\n34\n`, return `34`, and six lifecycle events across two owners.
The post-drop trap and budget-without-synthetic-drop controls passed.
Tests rejected malformed owner input and prevented widening of legacy
evaluator, C, CFG, passes, canonical text, and O1VM paths
(`$B/w5/owner-probe.log`). Clang 22.1.8 ASan/UBSan covered the emitted
products, reference C11, and rebuilt C runtime, not the compiler
(`$B/w5/owner-c-clang-sanitized.log`). The exact-pin AMD64 Archon bridge
emitted scalar control assembly and rejected v5 and downgraded owner table
or opcodes before MachineIR lowering (`$B/w5/owner-probe-archon-pinned.log`).

In W5's coherent current-source narrow GCC C++20 SDK run, private
`make_builtin_mir` rejected v5 or downgraded owner data with
`ON1X_SDK_UNSUPPORTED` and a null output. Public `on1x_sdk_mir_deserialize`
rejected v5 with `ON1X_SDK_UNSUPPORTED`, and a legacy payload with forged
owner opcode with `ON1X_SDK_ERROR`; neither published a snapshot and both
classified the input as malformed. Scalar v1 wrapper and public snapshot
controls passed (`$B/w5/owner-probe-sdk-retry.log`). This is scoped SDK
rejection, not SDK owner execution or plugin qualification. The initial
generated `on1x.mir.nominal_owner` CTest also passed
(`$B/generated-ctest-integrated.log`).

### Native KernelBind fixture

W6 targets `mulib.priceopt` candidate selection on hosted Linux AMD64:
optional Exotract/Lua, reviewed static KernelBind, immutable snapshot
arguments and value results, with independent C boundary and scoring
oracles. Test authority is not production approval. This does not
expand portable `stdffi`, native packages, owner transfer or MIR/VM admission.

The new [fixture](../tests/integration/price_candidates/check.py) passed
**134** exact-rational cases (**92** successes and **42** rejected outcomes),
plus the separate ABI/ownership/status probe, using historical
`build/harness-on1x-make/{on1xc,libstone-moonstone/exotract/exotract}`,
the current source driver/header, and GCC 16.2.1. Discovery only, raw unsafe
spelling and ordinary MIR ingress stayed non-callable. No review, wrong
authority and changed flags denied publication. The successful authorized
path used an ephemeral test Ed25519 authority and one real
`MULIB_IMPL_PRICEOPT` TU plus libm. See `$B/w6/archived.log`. This is not
current-compiler evidence. The fresh exact-pin Exotract-only build stopped
at an unavailable Aurocks Perl driver in ExoML
(`$B/w6/moonstone-{config,build}.log`). A separate GCC 16.2.1 Release
Exotract with Lua/libclang/ExoML rebuilt from the pinned-source projection
plus historical Perl Aurocks (`7878cb91cecf01926e8f0fe441bedd6e19ca03ac`).
The extractor SHA-256 is
`7499bbd99c39b827c219660bf52a594c7a4e7f930f1003f1a12c9bf40cf3b4ae`;
see `$B/w6/moonstone-projected-{config,build}.log`. This is not a pristine
pinned dependency build.

W6's later preflight paired that rebuilt extractor with the historical
`build/harness-on1x-make/on1xc` (SHA-256
`41365aba678816c0b17fa98426b3b0c322eb335efba7a4f63fb5c5664a17d775`),
using GCC 16.2.1, Clang 22.1.8 resource headers and OpenSSL 3.6.4:

```sh
python3 -B on1x/tests/integration/price_candidates/check.py \
  --on1xc build/harness-on1x-make/on1xc \
  --exotract "$B/w6/moonstone-projected/exotract/exotract" \
  --artifacts "$B/w6/latest-preflight-artifacts" --cc gcc
```

The same 134 exact-oracle cases passed (92 expected scores and 42 expected
rejections), with separate ABI/status probes, denied raw unsafe and direct
MIR/VM ingress, and fail-closed review/publication controls.
See `$B/w6/latest-preflight.log` and `$B/w6/exercised-inputs.sha256`.
A Clang ASan/UBSan run of the generated C application and ABI probe
passed the same product corpus (`$B/w6/sanitized-preflight.log`);
it did not instrument the historical compiler and preceded the direct
VM rejection assertion. Neither preflight used a freshly built On1x.

**Fresh fallback compiler, manual focused runs.** W6 repeated the fixture
with a private copy of the COORD-006 fallback Release compiler (SHA-256
`9a294afbc027744febec6dda2a5d302d7bf60c140f9e52d0f4b2c06bd16ce363`),
the current driver/harness/header and the projected extractor above. Both
GCC 16.2.1 and Clang 22.1.8 ASan/UBSan passed all **134** cases (**92**
scores, **42** expected rejections), the separate C ABI and untouched-output
checks, and the negative admission cases. The logs record full commands
and per-artifact hashes (`$B/w6/current-gcc.log`,
`$B/w6/current-clang-sanitized.log`). Sanitizers covered the generated
application, canonical one-TU kernel, adapter and both native probes;
they did not instrument the compiler, extractor or Python oracle. This
fallback compiler had built-in ExoML/Exotract/Lua disabled; the test used
an explicit external extractor. Neither run exercised the later integrated
compiler binary, the generated scanner, the optional CTest registration,
production authority, or the `optimize` entry point.

**Initial integrated generated compiler, manual focused run.** The
pre-relocation-fix generated compiler also passed the **134** exact-oracle
cases (**92** scores, **42** expected rejections) and C ABI/status checks
with W6's external projected extractor (`$B/integrated-generated-priceopt.log`).
The logged program SHA-256 is
`dd5ab08944072d208157307706977b1c4b2a4841f11cb190927ce8105a4ac2f2`.
This demonstrates the same reviewed static C11 application profile on that
compiler. It did not enable internal ExoML/Exotract/Lua, exercise the
optional CTest registration, or qualify the final installed build.

### Independent fallback install and move

W7 ran `python3 -IB on1x/tools/qualify-installed-sdk.py --build "$B/fallback" --work /mnt/nvme/on1x-sdk-w7.OXqJnTHw/fresh-fallback --require-qmr`.
It installed under `original`, renamed to `moved prefix` outside MuLIB,
then used a clean environment and working directory. Installed-only C++17
`On1x::Project` passed **1/1**, isolated Python manifest validation passed,
and installed CLI validated a manifest and exact v4 lock. Temporarily
removing the installed project validator made manifest validation return
structured `UNSUPPORTED`.
The relocated project selected the installed Mune policy (`-O1 -g`) and
printed `42`; moved QMR→Mune returned `role = "moved"`.

This is the initial GCC 16.2.1 Release fallback configuration with QMR on
and optional Cephyr runtime off; the pre-owner compiler SHA-256 is
`a28a712960a07e98fee6acc8ee5801342fb3ee236ed1bcc2be795e332283d7bd`.
See `$B/w7/relocate-fresh-fallback.log` and adjacent `-config.txt`,
`-before.sha256`, and `-after.log` records with the same basename. This
is not the final integrated SDK, generated scanner, Archon install, or
external Cephyr runtime relocation evidence.

## This task — principal's combined runs

An unavailable provider, absent CTest registration, skip, configure failure,
or historical binary is not a pass. Counts below are per run, not summed.

### Reproduce the combined configurations

Linux AMD64 (`7.2.6-arch2-1`, glibc 2.44), GCC/G++ 16.2.1, GNU binutils
2.47, Python 3.14.7, Perl 5.42.2 and CMake 4.4.3 / GNU Make 4.4.1:

```bash
root=$PWD
libstone="$root/on1x/third_party/libstone"
cephyr="$root/build/kernel-load-cephyr-prefix/lib/cmake/Cephyr"
export TMPDIR="$B/tmp" PYTHONDONTWRITEBYTECODE=1
mkdir -p "$TMPDIR"
unset ON1X_LIBSTONE_ROOT CEPHYR_LIBRARY
common=(-S on1x -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Release
        -DBUILD_TESTING=ON "-DON1X_LIBSTONE_ROOT=$libstone"
        "-DCephyr_DIR=$cephyr")
cmake "${common[@]}" -B "$B/fallback" -DON1X_USE_FALLBACK_SCANNER=ON
cmake --build "$B/fallback" -j 3
ctest --test-dir "$B/fallback" --output-on-failure -j 3

# Generate the exact-pin AMD64 backend-only package and the mixed-compiler
# Aurocks host binary using the dependency provisioning runbook.
python3 -IB on1x/tools/prepare-type-sdk.py --output "$B/satie-euf-fixed"
cmake "${common[@]}" -B "$B/generated" -DON1X_REQUIRE_LIBSTONE=ON \
  "-DON1X_AUROCKS_HOST_EXECUTABLE=$B/w7/aurocks-native/aurocks" \
  -DON1X_WITH_CEPHYR=ON -DON1X_WITH_ARCHVIRT=ON \
  "-DON1X_ARCHVIRT_ROOT=$root/build/on1x-archvirt-prefix" \
  -DON1X_BUILD_MIR_ARCHON=ON -DON1X_ARCHON_ENABLE_SDK_ADAPTER=ON \
  -DON1X_ARCHON_TARGET_TRIPLE=x86_64-unknown-linux-gnu \
  "-DArchonBackend_DIR=$B/w7/archon-native-prefix/lib/cmake/ArchonBackend" \
  -DON1X_WITH_CHISEL=ON -DON1X_WITH_NEROTYPICAL=ON \
  "-DON1X_SATIE_ROOT=$B/satie-euf-fixed" \
  -DON1X_WITH_FIRESTONE_HIR=ON -DON1X_WITH_NOVOSTONE=ON -DNOVOSTONE_PCC_USE_SATIE=OFF \
  -DON1X_WITH_EXOML=OFF -DON1X_WITH_EXOML_EXTRACT=OFF \
  -DON1X_WITH_EXOTRACT=OFF -DON1X_WITH_EXOTRACT_LUA=OFF \
  "-DON1X_LINKED_SDK_EXTENSION=$root/on1x/sdk/tests/linked_extension.c"
cmake --build "$B/generated" --target on1x_stdffi_tables -j 1
cmake --build "$B/generated" -j 6
ctest --test-dir "$B/generated" --output-on-failure -j 3
```

Both use MEMGraph, Firestone regex, Syren, Wetstone, QMR, SDK with the
portable registrar map, the Linux plugin worker, and native PPX defaults.
The fallback profile excludes runtime Cephyr, ArchVirt, Archon, independent
type/memory checkers, Novostone and Firestone checked-HIR optimization.
The generated profile additionally uses the pinned Archon backend-only
archive (SHA-256 `bfe88ccfe1d139cada2ac1842a382b3c98038e5b4fd8a4775b2c468dafb548b1`)
and an existing ArchVirt install (shared library SHA-256
`827ea0c73787d9f5072c6e6ba8c5414fd9fc43ac77ec91d79ade71e0d21e825d`;
source revision not established). For exact cache values and transient tool
failures, see `$B/{fallback,generated}/CMakeCache.txt` and the named logs.
This is qualification of On1x, not a MuLIB-root or cross-platform suite.

The extended `$B/compat-ffi` configuration uses the same generated Release
options, but additionally enables `ON1X_WITH_EXOML`,
`ON1X_WITH_EXOML_EXTRACT`, `ON1X_WITH_EXOTRACT`,
`ON1X_WITH_EXOTRACT_LUA` and `ON1X_USE_YELLOWSTONE_LINK`. It selects
`$B/libstone-ffi-compat` for `ON1X_LIBSTONE_ROOT`: links to the pinned
checkout except a real `scripts/` directory and a materialized `moonstone/`
from `git archive 76d24a7…`, so ExoML's relative `../../scripts/aurocks.pl`
resolves to the explicitly projected bootstrap. The bootstrap is the
`7878cb91cecf01926e8f0fe441bedd6e19ca03ac` Perl Aurocks script with
SHA-256 `9f192e8175dcdb54993fff8c608808c0ab2866f39e644f4008528b61f8150a78`;
`YELLOWSTONE_LDSCRIPT_AUROCKS` explicitly selects the same script. The
symlink-only Moonstone attempt failed because `..` resolved through the
original checkout. Neither projection modified checked-in dependency files.

```sh
cmake --build "$B/compat-ffi" --target on1x_stdffi_tables -j 1
cmake --build "$B/compat-ffi" --target on1xc exotract \
  on1x_backend_capabilities_test on1x_mir_nominal_owner_tests -j 2
ctest --test-dir "$B/compat-ffi" --no-tests=error --output-on-failure -j 2 \
  -R '^on1x\.(integration\.price_candidates|backends\.(capabilities|yellowstone)|processor\.(generator|snapshots)|mir\.nominal_owner|stdlib_project)$'
```

The selected tests passed **7/7** (`$B/compat-ffi-focused.log`), including
the optional PriceOpt CTest against the built `exotract` and the actual
Yellowstone-generated registrar map. Earlier all-target build attempts in
this profile hit separate GCC internal compiler errors in `inherent.cpp`
and `test_archvirt_cache.cpp`. See `$B/compat-ffi-{build,build-materialized,build-retry,build-focused}.log`.
This is a documented compatibility bootstrap, not a clean dependency-only
configuration.
The full retry used `cmake --build "$B/compat-ffi" -j 2` and
`ctest --test-dir "$B/compat-ffi" --output-on-failure -j 3` with
`ulimit -c 0` (`$B/compat-ffi-build-full.log`, `$B/compat-ffi-ctest.log`).
The configure settings are in `$B/compat-ffi/CMakeCache.txt`.

### Results and limits

- The full pre-owner fallback run (`$B/fallback-ctest-preintegration.log`):
  **251 passed, 2 failed, 1 skipped (254)**. Fixed two stale assertions
  without changing language/catalog policy: `category` passed more than
  one mutually exclusive compiler mode, and `mulib` expected old Mune
  and Elvi exported-symbol counts. Both then passed together
  (`$B/fallback-corrected-fixtures.log`).
- First integrated fallback (`$B/fallback-ctest-integrated.log`):
  **249 passed, 5 failed, 1 skipped (255)**. Failures: `formatter`,
  `stdlib_project`, `grammarembed`, `project_driver`, `processor.generator`.
  The logs include intermittent GCC internal errors and unexpected
  Python startup/On1x failures; the first run is not overwritten by a retry.
- Final fallback Release (`$B/fallback-ctest-final.log`):
  **254 passed, 1 failed, 1 skipped (256)**. `project_fetch` failed once
  when a subprocess exited on SIGSEGV, then passed by itself with
  `ctest --test-dir "$B/fallback" --rerun-failed -j 1 --output-on-failure`
  (`$B/fallback-rerun-failed.log`). The skipped live cgroup test is not a
  pass: `ON1X_TEST_POD_CGROUP_PARENT` did not name an exclusive delegated
  cgroup v2 parent (`$B/cgroup-skip-reason.log`). A separate install and
  move outside the checkout using
  `python3 -IB on1x/tools/qualify-installed-sdk.py --build "$B/fallback" --work NEW_DIRECTORY --require-qmr`
  passed again (`$B/fallback-final-relocation.log`).
- First integrated generated (`$B/generated-ctest-integrated.log`):
  **282 passed, 8 failed, 1 skipped (291)**. `relational` and `relational_hir`
  used fixture expressions that now attach MIR v4 directives and properly
  reject in the scalar v1 mapping; the fixtures now test scalar success
  and explicit v4 rejection. `install`, `project_build` and
  `backends.mir_archon_native_build` exposed missing installed archive and
  manifest/cache-path bugs; the driver now receives the selected manifest in
  both trees and the installed runtime archive. `mir_borrow_products`,
  `passes` and `project_driver` had separate process/compiler crashes.
  This run already passed the new owner, provider reporting, native
  differential, and both original regression tests.
- Generated follow-up after install and CLI relational fixes
  (`$B/generated-ctest-release.log`): **224 passed, 67 failed, 1 skipped
  (292)**. A generated compiler artifact that had passed the focused
  `install`, native project, owner, registry, Satie and CLI relational
  checks later started returning SIGSEGV even on
  `on1xc --check on1x/examples/mir.on1x`. GDB could not read the faulting
  instruction in the executable's `infer_types` call path
  (`$B/generated-check-gdb.log`, `$B/generated-crash-insn.log`).
  The independently built fallback compiler still checked the same source.
  This is not a passing release suite or a diagnosed compiler source bug.
  The failure list is preserved in `$B/generated-ctest-release-failures.log`.
  CMake's unchanged link inputs produced a new executable that passed the
  scalar smoke (`$B/generated-relink.log`). Its SHA-256 is
  `ad522e7cec60b02842e1ef4465deec74133e48d318f9553a922e1a8659f11bf8`.
  The remaining stale random relational fixture also stopped attaching
  directives and passed its independent oracle after a targeted rebuild
  (`$B/generated-relational-direct.log`).
- Relinked generated Release (`$B/generated-ctest-relinked.log`):
  **290 passed, 1 failed, 1 skipped (292)** with the relational fixture fix and
  `ulimit -c 0`. The remaining `on1x.mir` failure was a host `ld` subprocess
  exiting on an illegal instruction. It passed by itself with
  `ctest --test-dir "$B/generated" --rerun-failed --output-on-failure -j 1`
  (`$B/generated-relinked-rerun-failed.log`). The live cgroup fixture was skipped.
  `on1x.install` passed, including actual link/run from moved Archon
  and VM prefixes and hashes of the moved manifest/runtime archive. In a
  separate check, `python3 -IB on1x/tools/qualify-installed-sdk.py --build
  "$B/generated" --work NEW_DIRECTORY --require-qmr` passed after an
  independent move outside the checkout (`$B/generated-final-relocation.log`).
- Generated compatibility Release (`$B/compat-ffi-ctest.log`):
  **308 passed, 4 failed, 1 skipped (313)**. PriceOpt, Exotract,
  Yellowstone and install passed. Three isolated fixture checkouts
  omitted `on1x_toolchain.py`; the discovery review-candidate fixture also
  needed `on1x_ffi_trust.py`. The dynamic-link fixture still expected
  the framed IPC resource to work in a host DSO although the shipped ABI
  is static-only. The fixture now executes the static control and requires
  no output from either available DSO driver on rejection. No product
  admission policy changed. The four corrected fixtures passed **4/4**
  with `ctest --test-dir "$B/compat-ffi" --rerun-failed --output-on-failure -j 1`
  (`$B/compat-ffi-rerun-failed.log`). The earlier direct retries are in
  `$B/compat-ffi-fixture-direct.log` and `$B/compat-ffi-discovery-retry.log`.
  No second full compatibility run was performed; the first and its
  failure list remain in `$B/compat-ffi-ctest{,-failures}.log`.

`$B/final-artifact-identities.sha256` records the three tested compiler
binaries, the optional extractor and the two independent moved compilers.
Both moved compilers matched their build-tree binaries byte for byte.
The integrated compatibility extractor also matched W6's separately built
projected extractor.

An all-pristine build using current pinned dependency sources remains
unqualified. The broad ExoML/Exotract build requires the deleted Perl parser
generator. A freshly rebuilt GCC Cephyr package with a historical QLD
bootstrap and local Valijson overlay passed the independent AMD64 object
and 15 FFISpec fixture checks. The first full-header scan exited on a
segmentation fault; an independent fresh output directory then completed
with one recipe and 2077 catalogue entries (`$B/cephyr-host-fresh-scan.log`).
The latter are catalogue outcomes, not independently reviewed or admitted
foreign calls. The rebuilt library SHA-256 is
`7e2519ddc78980025f4e4e54c2a2c61c7ecff9113c401d2bd883781659726801`.
New input packages do not inherit the old suite's results. AMD64 is the
only native target for which this task exercised On1x native artifacts.
The second fresh Cephyr build used the [tracked Valijson layout projection](dependency-provisioning.md#build-host-cephyr-and-optional-providers)
and the same explicit historical QLD generator. Its package SHA-256 is
`30fdd0dfd33cc3ff5f67061359cc7845b13c7afad228633f552a6c65d8a16aea`
(`libcephyr.so`); it passed the same separate AMD64 C-object controls,
15 FFISpec checks, and the full header scan in `$B/cephyr-projection-{clamp,ffispec,scan}.log`.
The projection build initially hit a GCC internal error, then built and
installed with `-j 2`. Neither package was substituted into the successful
portions of the combined runs.

Clang 22.1.8 ASan/UBSan Debug used `CMAKE_C_FLAGS`, `CMAKE_CXX_FLAGS` and
`ON1X_TEST_C_FLAGS` with `-fsanitize=address,undefined
-fno-sanitize-recover=all -fno-omit-frame-pointer`, and `-g0` for the Debug
flags. It used the fallback scanner/usual build-host Cephyr, and disabled
native PPX, dynamic SDK plugins, Syren, Wetstone, QMR and native providers.
The first builds stopped at Clang front-end crashes in
`sdk/src/compiler/Compilation.cpp` (SDK on) and
`hir/On1xMETA/partial_eval.cpp` (SDK off). The SDK-on retry passed **3/3**:

```sh
cmake --build "$B/sanitized" --target on1x_mir_nominal_owner_tests \
  on1x_mir_tests on1x_vm_tests on1x_runtime -j 1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir "$B/sanitized" -R '^on1x\.(mir\.nominal_owner|mir_hir|vm\.opcodes)$' \
  --no-tests=error --output-on-failure
```

This instruments the focused C++ fixtures and linked frontend, MIR, VM
and SDK code, plus the emitted C and independent oracle linked to the
instrumented C runtime. The compiler driver, dynamic worker, native
assembly, and external Cephyr/extractor weren't exercised under these
sanitizers. Other instrumented runs covered W4's verifier, W5's generated
and handwritten C with runtime, and W6's C product, kernel, adapter and
native probes. See `$B/{sanitized,sanitized-core}-build.log`,
`$B/sanitized-build-retry.log` and `$B/sanitized-ctest.log`.

### Remaining release gates

1. Repeat all three full configurations from clean build directories on a host
   where the compiler, runtime and linked executable remain stable. Retain
   compiler hashes before/after and require a single passing combined run
   per configuration; passing focused retries don't erase failures.
2. Port or pin the upstream Cephyr/ExoML/Yellowstone Perl generation
   dependencies explicitly. Rerun full FFISpec plus the integrated
   ExoML/Exotract/Lua tests and Yellowstone map tests without an undeclared
   bootstrap. Port or keep explicitly disabled Archon's source translation
   stage; the pinned native C archive alone does not cover it.
3. Run live cgroup enforcement on a delegated host; repeat native
   qualification for each additional host/target with that target's
   actual executable, ABI checks and an independent oracle. Archive/ELF
   generation is not execution. Repeat optional-runtime relocation with
   its exact external dependencies intentionally available or withheld.
4. For any reviewed FFI release claim, use the real review authority and
   retain the approved subject, pinned ABI and execution evidence. The
   ephemeral PriceOpt test key does not authorize deployment.
5. If expanding the nominal-owner profile, add a separate checked-HIR
   lowering and negative boundary tests before permitting any owner wire,
   SDK, VM, native, foreign, CFG, borrow or cleanup semantics. The current
   named C++ entry points do not authorize those additions.

### Worker handoffs

The coordinator integrated the changes and owned the combined-run ledger,
provisioning retries and release disposition. Run-local handoffs are under
`$B/w1/mail` through `$B/w8/mail` and `$B/coord-mail`.

| Worker | Contribution |
| --- | --- |
| W1 | Reproduced the stdlib staging failure and checked declared inventory and relocated import behavior. |
| W2 | Compared old and fresh On1xMETA behavior, normalized parser provenance, refreshed snapshots, and tested the generated and fallback paths. |
| W3 | Implemented and checked linked SDK backend reporting across portable and Yellowstone maps, including the native provider and target mismatch. |
| W4 | Defined and verified the in-memory owner MIR slice and tested malformed input and legacy boundary rejection. |
| W5 | Implemented the named evaluator and C11 routes and compared them against a handwritten C lifecycle oracle. |
| W6 | Qualified the static PriceOpt candidate-selection fixture against an independent exact oracle, ABI/status probe and admission negatives. |
| W7 | Provisioned exact-pin host Aurocks and AMD64 Archon backend artifacts; independently installed, moved and ran the fallback SDK and project fixtures. |
| W8 | Reconciled current capability, completion and roadmap claims with dated evidence and archived older claims. |
