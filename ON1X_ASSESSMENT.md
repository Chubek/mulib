# On1x assessment

## Snapshot

- **Subject:** `on1x/`, the On1x compiler, language implementation, SDK, runtime, tooling, addons and tests in this repository.
- **Revision assessed:** HEAD `0a323d2d` ("Record 17 passing parent-tree integration tests", 2026-11-27). This is a documentation-only commit; the executable code is the same as parent `aa48bf57`.
- **Declared version:** `0.1.0` (`on1x/CMakeLists.txt:2`).
- **Overall verdict:** **an ambitious, unusually well-instrumented research/alpha compiler, but not a self-contained or production-ready language distribution in its current tree.**
- **Confidence:** high for build/reproducibility and current verification coverage; moderate for whole-code quality, because a complete security audit, formal proof, cross-platform matrix and full historical regression suite were not rerun here.

The strongest parts are the compiler architecture, explicit trust boundaries, negative/adversarial tests and unusually honest qualification reports. The decisive weakness is distribution state: a clean checkout cannot configure the full compiler without deleted assets, and the only currently passing full-compiler profile deliberately runs a narrow 17-test module-integration suite against an external, trusted checkout.

---

## 1. What On1x is

On1x is described by `on1xc --help` as a small strict ML-family language targeting ISO C11, but the implementation is considerably broader than that description:

| Layer | Current design |
|---|---|
| Front end | Generated ASDL/Aurocks scanner or fallback scanner; C++ parser, loader, imports, macros, hygienic local syntax, PPX, staging, inference and semantic checking. |
| Checked HIR | One canonical `on1x::Unit`; transactional optimization and rechecking, canonical control/value lowering, MEMGraph lexical-slot and bounds analysis, optional Firestone/Novostone/On1xMETA rewrites. |
| MIR | Versioned serialized MIR v1–v4, a verifier, scoped rewrite-policy banks and a typed scalar subset. Direct C11 remains the richer reference path; unsupported HIR features fail rather than silently fall back. |
| Products | Direct C11, MIR C11/text/CFG/reference evaluator, VMWeave VM, opt-in Archon native assembly, Cephyr and ArchVirt paths. |
| Native support | A private generated-program runtime, plus independently versioned **On1xRT 1** for embedding and **SDK 2** for compiler extensions. |
| Tools | Project/build drivers, formatter, diagnostic catalog, KernelBind/ExoML/Exotract contracts, LSP, editor syntax packages, MCP addon and benchmarks. |

This is a coherent layered design. The separation between *observation*, *type/body admission*, *foreign-contract review* and *executable publication* is repeatedly and correctly emphasized in `on1x/hir/README.md`, `on1x/mir/README.md`, `on1x/sdk/SECURITY.md` and `on1x/artifacts/MODULE_SEMANTICS.md`. Optional optimizers and backends are generally off by default and are gated by explicit CMake/CLI selection.

Its scope is also its principal engineering risk: there are 1,442 tracked files under `on1x/`, 347 under `on1x/sdk/`, 311 under `on1x/tests/`, 146 `add_test(` call sites in the central CMake file alone, a 1,550-line `on1x/CMakeLists.txt`, and several very large implementation units (`tools/on1x-project.py` 180 KB, `src/parser.cpp` 143 KB, `src/loader.cpp` 135 KB, `src/infer.cpp` 120 KB, `src/cli.cpp` 104 KB, `runtime-sdk/src/On1xRT.c` 114 KB). This is a substantial system to review, configure and keep synchronized.

---

## 2. Evidence inspected and reproduced

This assessment is based on the tracked tree at HEAD, the current CMake configuration, subsystem READMEs and qualification artifacts, plus the following local checks.

| Check | Result | Interpretation |
|---|---|---|
| `cmake -S on1x -B <fresh> -DBUILD_TESTING=ON` | **Fails** at `on1x/CMakeLists.txt:43`: `The full compiler needs removed stdlib/diagnostics/catalog.json`. | The ordinary/full profile is not buildable from tracked files alone. |
| Fresh `-DON1X_MODULE_CORE_ONLY=ON` Debug build | **4/4 CTests passed** in 0.48 s. | The standalone module algebra, transparent-permit and graph/wire tests work, but this profile explicitly does not build `on1xc`. |
| Fresh external-root configure | **Succeeded**; `ctest -N` registered exactly **17 tests**. Omitting `ON1X_STDFFI_CEPHYR_LIBRARY` fails at `hir/FFIHandler/CMakeLists.txt:34`. | A full compiler configuration is possible only with an explicitly selected external `on1x/stdlib`, pinned LibStone and a build-host libcephyr. |
| Existing current-source external-root Debug build | `cmake --build` was up to date; **17/17 CTests passed** on rerun in 14.93 s. The recorded HEAD-parent run was 17/17 in 14.70 s. | Strong evidence for the narrow module-integration slice, not for the general regression suite. |
| Smoke: `on1xc --emit-c --no-prelude on1x/runtime-sdk/tests/choose.on1x` | Generated C compiled with `-std=c11 -Wall -Wextra -Wpedantic -Werror` and ran with exit 0. | The direct-C11 path works for a small scalar program when a built compiler is available. |
| Markdown local-link scan | 121 tracked docs checked; 59 references to 48 missing local paths. Most point to removed `on1x/docs/`, `on1x/examples/`, `on1x/stdlib/` or deleted report/result files. | Current documentation is not navigable as a standalone reference. Some links are intentionally historical, but that is not consistently labelled. |
| Working-tree hygiene | Six untracked, zero-byte `.oml` files in `on1x/src/logistics/`: `import-graph.oml`, `module-dependency-graph.oml`, `prune-import-graph.oml`, `serialize-graphs.oml`, `simplify-graphs.oml`, `visualize-graphs.oml`. | Visible work-in-progress placeholders; they are not implementation evidence at HEAD. |

Reproduction of the two supported profiles:

```sh
# Standalone semantics; builds no compiler.
cmake -S on1x -B build/on1x-core \
  -DON1X_MODULE_CORE_ONLY=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/on1x-core
ctest --test-dir build/on1x-core --output-on-failure

# Narrow full-compiler integration profile. The external stdlib must be a
# pinned, explicitly trusted checkout in the original REPO/on1x/stdlib layout.
cmake -S on1x -B build/on1x-integration \
  -DON1X_MODULE_STDLIB_ROOT=/path/to/trusted/mulib/on1x/stdlib \
  -DON1X_LIBSTONE_ROOT=/path/to/libstone \
  -DON1X_STDFFI_CEPHYR_LIBRARY=/path/to/libcephyr.so \
  -DON1X_WITH_WETSTONE=OFF -DON1X_WITH_SYREN=OFF \
  -DON1X_WITH_PROJECT_QMR=OFF -DON1X_WITH_KERNEL_NATIVE=OFF \
  -DON1X_BUILD_DYNAMIC_SDK_PLUGINS=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build/on1x-integration
ctest --test-dir build/on1x-integration --output-on-failure
```

The external profile used in `on1x/artifacts/MODULE_ALGEBRA.md` and `MODULE_SEMANTICS_IMPLEMENTATION.md` executes Python from the external checkout during the build. That checkout is trusted build input, not merely data. LibStone is better pinned: `.gitmodules` records commit `4607ce58…`, while `on1x/cmake/ModuleAlgebra.cmake` also checks a fixed `LibDSL.hpp` SHA-256 and the OML rule digest.

Not performed here: a from-scratch full external build, a full historical regression suite, an independent penetration test, fuzzing, formal verification, or cross-compiler/cross-target execution. Therefore this is an engineering assessment, not a certification.

---

## 3. Strengths

1. **Architecture and fail-closed boundaries.** Checked HIR is rechecked after private transactional rewrites; MIR is verified before dispatch; graph deserialization is an observation, not authority; unsupported MIR/VM/native routes reject before publication. The distinction between declaration discovery, reviewed foreign admission and native execution is explicit in `README.md:54-64` and `on1x/sdk/SECURITY.md`.

2. **Serious test design.** `on1x/tests/README.md` documents differential oracles, seeded randomized cases, strict generated-C compilation, malformed-wire negatives, ownership/leak counting, atomic-output preservation and sanitizer runs. Current module evidence includes 500 seeded graphs compared against an independent Floyd–Warshall reference, a 1,024-node chain, bounded aggregate-import rejection, ASan/UBSan CLI checks, 27 independently compiled SDK headers and a source-overwrite regression with symlink/FIFO destinations. These are more useful than simple happy-path tests.

3. **Security thinking is unusually explicit.** The dynamic SDK plugin worker is designed to establish namespace/seccomp/resource limits before `dlopen`, with no in-process fallback; On1xRT's authorized dynamic loader measures a sealed artifact and requires host approval; output publication is atomic; descriptors, wire frames and snapshots are versioned and bounded. `on1x/sdk/SECURITY.md:3` correctly calls this a design and test contract, not an audit claim.

4. **Honest qualification records.** `on1x/artifacts/MODULE_SEMANTICS_IMPLEMENTATION.md` records exact revisions, inputs, failures, exclusions and the limits of each test. It explicitly says the clean default profile cannot build, that G3/G5 remain incomplete, and that no production-readiness claim is made. Benchmark docs likewise state that paired microbenchmarks do not establish general application speedups. The root README's "no production approval merely by shipping a test" warning is consistent with the evidence.

5. **Reproducible core and deterministic formats.** The core-only profile builds and passes from a fresh directory. Graph JSON/S-expression formats have schemas, canonical ordering, bounded sizes, source digests and deterministic round trips; the emitted C smoke test compiled cleanly under strict C11 warnings.

---

## 4. Principal findings and risks

| Severity | Finding | Evidence and impact |
|---|---|---|
| **P0** | **The full compiler is not buildable from a clean clone of this tree.** | Commits `40df0aaf` and `791ea779` removed 414 and 649 files respectively, including `on1x/docs/`, `on1x/examples/` and `on1x/stdlib/`. `on1x/CMakeLists.txt:40-44` still requires five deleted stdlib assets. The default configure reproduces the fatal error. The root `CMakeLists.txt` does not add `on1x/` as a subdirectory, so the documented root build does not cover it either. |
| **P0** | **Full-build reproducibility depends on untracked external inputs.** | The external profile needs a trusted MuLIB checkout with the original `REPO/on1x/stdlib` layout, a pinned LibStone tree, and a build-host libcephyr; it executes external Python at build time. A fresh configure without the Cephyr library fails at `hir/FFIHandler/CMakeLists.txt:34`. Only LibStone is pinned by a submodule/hash here. A clean clone cannot reproduce the result from tracked assets, and the external build path is a supply-chain trust decision. |
| **P1** | **Current verification is narrow, despite a large historical test inventory.** | The external profile sets `BUILD_TESTING=OFF` for the general suites and registers only 17 module-integration CTests. `on1x/tests/README.md` describes dozens of older suites, but fixtures depending on deleted stdlib/examples/docs are unavailable. The current report also notes two skipped LSP cases, one `Modules` case that errored on a missing `examples/energy-budget/expected.txt`, no full compiler installation, no general native-PPX regressions and no cross-target/native parity qualification. Dated 2026-11-19/24/26 reports are historical revision evidence, not current certification. |
| **P1** | **The module-semantics milestone is explicitly partial.** | The feature matrix in `on1x/artifacts/MODULE_SEMANTICS_IMPLEMENTATION.md:103-117` and its "Remaining gates" section (`:159-167`) leave static functors, genuine separate compilation, managed/generic/recursive transport, module-aware PPX negotiation and broader backend parity open. Only access, re-export, insertion and selection judgments are OML-authored; resolver, signatures, seals, dependencies and functors remain C++ or unimplemented. Six empty untracked OML placeholders reinforce the work-in-progress state. |
| **P1** | **Documentation does not describe the current checkout.** | Root `README.md` links to missing `on1x/docs/architecture.md`, `kernel-bind.md` and `ffi-migration.md`. `on1x/VERIFICATION.md` points to missing `docs/release-qualification.md` and `COMPLETION_MATRIX.md`. `on1x/grammar/README.md` points to a missing readable language grammar. CMake's non-external install branch still names absent `examples/operators.on1x` and `docs/kernel-load-time.md`. There is no current canonical language tutorial or support matrix in-tree. |
| **P2** | **Configuration and code surface are difficult to audit.** | Roughly twenty `ON1X_*` feature/dependency switches interact with generated front ends, several SDK versions, multiple backend admission gates and Linux/x86-64-specific optional components. Large monolithic source and CMake files make review and safe refactors expensive. SDK 2, frozen ABI-v1 extension behavior and On1xRT 1 are independently versioned surfaces that need a crisp long-term compatibility policy. |
| **P2** | **Security mechanisms are not a complete assurance case.** | Native callbacks and authorized AOT modules run with host authority and are not sandboxed; plugin isolation is limited to Linux x86-64 unprivileged builds; linked/in-process providers are trusted; the MCP HTTP frontend is explicitly not a production internet frontend. Tests are bounded regression evidence, not an independent penetration test. The trusted external stdlib executes build-time Python. |
| **P2** | **Performance evidence is targeted and historical.** | `on1x/benchmark/OPTIMIZATION_ROUND2.md` and `ZERO_COST.md` report paired microbenchmarks, byte-identical controls and uncertainty, but explicitly make no general runtime-improvement claim. These are useful optimizer gates, not evidence of application-level performance or compile-time scalability. |

A useful summary of the evidence asymmetry: **17/17 passing tests is a real result, but 17 tests cover a deliberately narrow slice of a system whose CMake source contains 146 `add_test` call sites and 311 tracked test files.** Passing those 17 tests should not be read as "On1x passes its test suite."

---

## 5. Maturity scorecard

| Dimension | Assessment | Rationale |
|---|---|---|
| Language/compiler architecture | **Strong for a prototype** | Layered checked HIR, verified MIR, explicit gates, no silent backend fallback. |
| Core direct-C11 path | **Promising, narrowly demonstrated** | Small scalar program and selected opaque/transparent module fixtures execute under strict C11; broader language regressions are not currently registered. |
| Module semantics work | **Substantial but incomplete** | Well-tested graph/algebra/signature slices; G3/G5, functors, separate compilation and OML completeness remain open by the project's own report. |
| Current verification | **Moderate, narrow** | 4/4 core-only and 17/17 external-integration tests reproduced at/near HEAD; historical suites are not available in this tree. |
| Build and reproducibility | **Weak** | Ordinary full configure fails; supported full build requires trusted external assets and host-specific dependencies. |
| Documentation/onboarding | **Weak** | Missing language/architecture/release docs and 48 broken local link targets in a simple scan. |
| Security posture | **Thoughtful design, unproven assurance** | Strong explicit boundaries and adversarial tests; no independent audit, no general sandbox claim, host-native trust remains. |
| Maintainability | **Mixed** | Excellent contracts and tests, but a very large, highly conditional, monolithic configuration/code surface. |
| Performance maturity | **Early** | Careful microbenchmark methodology; no general performance conclusion. |
| **Production readiness** | **Not ready** | Version 0.1.0; clean-checkout build, current full regression, installation/relocation, supported-platform matrix and release checklist are all incomplete or absent. |

---

## 6. Prioritized recommendations

### P0 — make the tree a reproducible product again

1. **Restore or replace the deleted assets.** Either vendor a minimal current `on1x/stdlib`, `on1x/docs` and `on1x/examples`, or refactor the full compiler so it no longer depends on them. Make `cmake -S on1x -B build/on1x` configure and build from a clean `--recurse-submodules` clone. Decide explicitly whether root `cmake -S .` should include On1x or document the separate build at the top of `README.md`/`INSTALL.md`.
2. **Pin all external inputs, not just LibStone.** Record the trusted stdlib tree hash, Cephyr artifact/version and generator/tool versions in a manifest. Verify hashes at configure time. Either eliminate build-time execution of external Python or isolate and audit that step; today "trusted external checkout" is an operational requirement without an in-repo pin.
3. **Publish a current release record.** Restore `COMPLETION_MATRIX.md` and `docs/release-qualification.md`, or replace `on1x/VERIFICATION.md` with a single HEAD-pinned report containing exact commands, commit hashes, build flags, results and exclusions. Add a Markdown link check to CI so historical links are labelled rather than silently broken.

### P1 — broaden and stabilize verification

4. **Run three CI profiles at every change:** core-only, external integration and, once assets are restored, the full regression suite. Add GCC/Clang, Debug/Release and ASan/UBSan variants where practical. Archive CTest logs and distinguish PASS, SKIP, expected-fail and blocked suites.
5. **Turn the feature matrix into a machine-readable supported/unsupported contract.** For functors, separate compilation, signature views, OML judgments, PPX graph transforms and each backend, state implementation status, executable evidence, fail-closed behavior and owner. Implement or remove the six empty OML placeholders.
6. **Qualify installation and relocation of the full compiler/SDK/On1xRT**, not just the core-only OML archive. Add cross-target and native-backend parity tests where claimed, and explicitly enumerate Linux/x86-64-only features.
7. **Commission an independent security review** of the plugin worker, On1xRT dynamic loader, native publication helpers and MCP/VM profiles, including hostile concurrent filesystem changes and fuzzing of wire formats. Keep the existing "design contract, not audit" disclaimer until that is done.

### P2 — reduce long-term maintenance cost

8. Split `on1x/CMakeLists.txt` into subsystem modules and reduce global conditional coupling. Consider decomposing the largest Python/C++ files behind tested interfaces.
9. Add a short, current language tutorial and supported-feature examples that are part of CI; preserve older qualification reports under a clearly named `historical/` section.
10. Define a compatibility/deprecation policy for SDK 2, legacy ABI-v1 hooks, On1xRT, MIR wire versions and generated C runtime ABI versions.

---

## 7. Bottom line

On1x should be judged as **two things at once**:

- **As a compiler research/engineering program:** impressive. It has a coherent multi-stage architecture, careful versioned contracts, unusually explicit trust boundaries, property/differential tests, adversarial publication tests and candid evidence reports. The current module graph/algebra work is genuinely implemented and verified within a bounded slice.
- **As a usable language distribution at this commit:** not yet. The ordinary build fails, the compiler requires an external trusted library that is not pinned in this tree, the current full-compiler suite is only 17 narrow tests, major module features and G3/G5 acceptance gates remain open, and the canonical user/verification documentation is missing or stale. The project's own reports and root README correctly withhold production approval.

The highest-value next step is not another feature: it is **making a clean checkout build, documenting exactly what that build contains, and restoring a current full-suite CI baseline**. Until then, every broader claim about correctness, backend parity, security or performance must remain explicitly scoped to the particular historical revision and configuration that produced its evidence.
