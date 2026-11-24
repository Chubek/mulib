# On1x FFI retirement manifest

**Historical audit, not a guide to the current compiler.** For current workflows
see [foreign interface migration](ffi-migration.md). Inventory baseline:
MuLIB `d890abb6e024091b776050964d25ed0f117c4a14`; its LibStone gitlink is
`e10ac69eed7baf91b06d15c4e0d58360097cb5c4`. A gitlink is not a populated
checkout or evidence that a tool was run.

Retire an item only after its owner hands off the necessary behavior and W11
independently qualifies old/new parity on the integration revision. The
coordinator must authorize the removal. Re-run the affected tests and clean
build/install afterwards. An unsupported replacement for a previously
qualified workflow is a blocker, not parity; any breaking change needs an
explicit release decision.

## Commands, implementations and build options

| Active baseline path | Destination or retained behavior | Retirement dependency |
| --- | --- | --- |
| `on1xc --kernel-imports INPUT` | Preserve dependency-first source request inspection, local alias collisions/source spans and no PPX execution. Keep the spelling as a compatibility frontend if still needed. | W06/W10 confirm request and driver contracts. |
| `on1xc --emit-c --kernel-c11` / `--kernel-stage`; `include/on1x/kernel.hpp`; hardcoded loader, nominal, infer, verifier, C11 and MEMGraph branches | One descriptor-driven checked-HIR and C11 path. Inspection/staged C is not reviewed admission; lexical `unsafe`, direct calls, left-to-right evaluation, and rejection by unsupported backends remain mandatory. | W06 owns canonical ingress and transitional header until handoff; W07–W09 replace emitter/profile logic. `tests/test_kernel_ipc_hir.cpp` and all header consumers must be ported. |
| Seed `stdffi/clamp.ffi`, FFIHandler CMake references to `stdlib/ffi/mulib/clamp.ffi`, `ffi_scalar.hpp` | `stdffi/mulib/` generated documents and index, `stdffi/overrides/mulib/` authored semantics, one typed registry. The baseline contains the seed at `stdffi/clamp.ffi`; the old CMake path is already absent. | W04/W05 change payload, dependency tracking, install and tests. Failure to find a source is not retirement. |
| `on1x-build.py --inspect-kernels`; `on1x-exotract.py list`, `list-kernels`; `on1x-contracts.py list`, `lint` | Target-qualified standard inventory/report with explicit reasons and canonical names; no compiler admission from an inspection report. Projected declarations and authored assumptions must be distinguishable. | W03/W04/W10, plus consumers below. W10 proposes `--inspect-ffi` and compiler source-request reporting via `--ffi-imports`; do not treat them as qualified aliases yet. |
| `on1x-build.py --kernel-bind`, `--kernel-dynlib-compiler`, `--kernel-review-candidate`, `--kernel-review{,-key,-signature}` | One native planner and one FFI policy; preserve any necessary aliases as frontends. Explicit host/Cephyr choice, static vs DSO subject binding and old fail-closed behavior. Keep independently authorized safe import distinct from lexical unsafe. | W10 hands off native tool/CLI paths after W06–W09 profiles pass. W11 qualifies static, dynamic, mixed compiler, trust negatives and publication. |
| `on1x-build.py --exotract`, `--resource-dir`; `contracts-exotract/kernel-discovery-v1.lua`; `on1x-bind extract` / `normalize-decl` | Cephyr resolution with explicit target/tool/headers, bounded C11Interface reading and generation-time provenance. Do not accept legacy CDecl dumps as equivalent evidence or silently swap declaration providers. | W01–W04 generation and W10 application build; preserve live unrelated extraction users until migrated. |
| `on1x-exotract.py plan`, `build`, `wrap-inline`; `on1x-build.py --mulib-archive` / `--mulib-plan`; `on1x-contracts.py build-plan` | Native plan independent of extraction. Preserve raw C archive consumers, validated fixture companions, explicit static-inline forwarding and a way to inspect/reject stale or incompatible link inputs. A source bundle is not a drop-in replacement for an archive. | W10 transfers planner and consumer paths; coordinator decides whether the existing raw archive/wrapper CLI or wire compatibility is needed. W07 qualifies an actual inline On1x path; that does not alone replace C consumers. |
| `on1x-contracts.py snapshot`, `check`; `on1x-bind pin`, `check`, `explain`, `verify` | Generated declaration/provenance freshness, fail-closed override matching, and independent review separate from both. Do not coerce old snapshots, subject formats or Gate B evidence into new trust. Preserve useful strictness and tamper checks in the replacement. | W03/W04/W05/W10 replacements; historical reproduction stays documented against the old revision. |
| `on1x-bind emit-raw` (alias `emit`), `emit-safe`, `tools/contract_{admission,cdecl,json}.hpp` | Replacement needs more than reviewed clamp: exact integer/Bool/floating ABI, checked narrowing, immutable call-scoped/static string boundary, and Chisel `--admitted-extern` evidence still have consumers. Do not equate evidence for this facade with independent signed KernelBind approval. | W06/W07/W10 + coordinator: retain until parity or an explicitly scoped breaking decision. See [contracts](contracts.md), [strings](ffi-strings.md), and VMM below. |
| `ON1X_WITH_EXOML`, `ON1X_WITH_EXOML_EXTRACT`, `ON1X_WITH_EXOTRACT`, `ON1X_WITH_EXOTRACT_LUA`; `on1x-bind`, `on1x-exotract`, `on1x_extend_kernelbind` CMake targets | Remove only the On1x dependency edges after final consumer removal. Audit `MOONSTONE_BUILD_EXOML`, `MOONSTONE_EXOML_EXTRACT`, `MOONSTONE_BUILD_EXOTRACT`, and `EXOTRACT_ENABLE_*` cache wiring and `cmake/DependencyReport.cmake`. Keep separate Chisel, Syren, Cephyr, On1xMETA, and MEMGraph controls as applicable. | Principal owns `on1x/CMakeLists.txt`; W05 owns FFIHandler CMake; W10 owns packaging. No edits to upstream LibStone. |
| `share/on1x/{contracts-exoml,contracts-exotract,tools/*}` | Stage/install standard FFI descriptions and the actual native/runtime inputs and tools the selected build needs. Confirm no application runtime dependency on declaration extraction or a generator. | W10 + W11 installed-user test. Also audit native PPX custom-command dependencies on the contract script and Exotract catalogue in `on1x/CMakeLists.txt`. |

## Separate compatibility blockers

| Workflow | Active consumers and qualification needed before removal |
| --- | --- |
| **Standalone `on1x-bind emit-safe` v1–v3**, not a MuLIB module import | `tests/test_contracts_sdk.py` exercises scalar/void v1, Bool/f64 v2, and exact-width unsigned/f32 v3, reviewed and rejected inputs, separately compiled C/C++ implementations, optional Chisel, and actual application clients. Preserve the bounded external-header workflow (including type/range checks, all-or-nothing eligibility, identity/ABI drift negatives, and no output on gate failure). Consumers import the generated `.on1x` facade and link its `.c` adapter, passing `--memory-check --admitted-extern FILE` with `on1xc` or `on1x-build.py`; `tests/test_safety.py` also exercises the evidence reader. A registry generated only from standard MuLIB headers does not replace this workflow. |
| **Standalone `reviewed-immutable-string/v4`** | `tests/test_contract_strings.py` builds/runs C11 clients with separately compiled C and C++ implementations and the ordinary build helper. Preserve directional `borrowed_string` input and `static_string` result, non-null exact `const char *`, call-only input, independently established immutable process-lifetime result, and no retention/alias/transfer. Keep 2 MiB input / 4 MiB result bounds and exit 72 on null/limit failure. The optional `stdlib/vmm/tests/admission.py` checks real extraction and eligibility of `archvirt_version` but **rejects** emission from the shipped unreviewed seed; it is not a successful production admission. |
| **Review trust for the standalone routes** | `emit-safe` in [contracts](contracts.md) and [strings](ffi-strings.md) runs metadata, ABI/eligibility, Chisel use-site and executable gates against trusted operator inputs; `on1x.admitted-extern/v1`–`/v4` is **not** a signed certificate. Its metadata must never be promoted to independent signed FFI review. The replacement would need equivalent bounded external-header and artifact/use-site support with fresh authorization under the new policy. Until independently qualified or explicitly deprecated by an authorized breaking change, retain this route, including `emit-raw` and the required evidence reader/build flags. |
| **Installed raw archive tooling and ordinary consumers** | `on1x-exotract.py`, `on1x-contracts.py`, `on1x-mulib.py`, and the relevant native/build scripts are installed by the baseline; `on1x-bind` is an optional **build target**, not in that install list. `tests/test_exotract.py` independently compiles/runs C archive clients for JSON/Vec/Parsec, MicroJS, Mune, ElviScrape and QUIC/HTTP3, and tests ordinary On1x `--mulib-archive` consumers (JSON, Mune, ElviScrape, YANTLR, Shiraz, stdlib bridge) and flags/artifact drift. `tests/test_install.py` checks installed fixture builds, not full archive and wrapper parity. W11 needs actual relocated installed-driver use with the selected canonical inputs and separate C client compilation/link/execution. |
| **Raw static-inline forwarding for C** | `tests/test_inline_wrappers_sdk.py` validates forwarding symbols, exact prototype/record-member checks and a separate C11 consumer; this is not a new On1x call profile. Preserve `wrap-inline` behavior or provide equivalent native planner/link tooling, including invalid selection and ABI drift rejection, before deleting the installed frontend. |

## Code and data that must survive

- **Planner:** `on1x/stdlib/mulib/housekeeping.py` imports the manifest reader,
  alias/local-path checks and bounded JSON utilities from `on1x-contracts.py`
  and calls `exotract_catalog()`/`catalog()` to select recipes. `on1x-mulib.py`
  (`list`, `plan`, `bundle`, `audit`, `verify`), the ordinary build driver,
  On1xfile native kernels and native PPX builds depend on it even when the
  extraction options are off. Preserve manifest closure, explicit optional
  switches, required symbols, native companions/fixtures, links and compile
  definitions, selected-header path/digest checks, one TU per partition,
  dependency drift checks and source-bundle locks. For example, Mune selects
  `MULIB_IMPL_PARSER`; QUIC/HTTP3 select live-client switches and POSIX
  definitions. These are **build recipes**, not call-admission evidence. Do
  not discard or hand-guess them when replacing `exoml-core`/`manifest`
  routing.
- **ABI and trust:** retain independent boundary probes and exact C prototype
  and layout checks. In `on1x-exotract.py`, `kernel_implementation_plan`,
  `review_subject`/`review_candidate`, `authorize_review`, and the callable
  profiles combine reusable checks with obsolete CDecl/ExoML structure.
  Preserve strict bounded parsing and types, operator-selected DER Ed25519
  key, detached signature over original bytes, exact normalized subject
  comparison, per-module review sets, tool/input drift, and static/DSO
  artifact identity. Carry forward staged no-replace publication, rejecting
  incomplete or changed inputs and keeping exact DSO/load policy; no old
  signature authorizes a changed subject. Preserve C11/native helpers in
  `src/backend/cephyr_kernel.cpp` and the necessary `runtime/on1x_kernel_ipc.*`
  bridge or their qualified replacements. On1xRT embedding and trusted
  dynamic loading are separate from KernelBind.
- **Sidecars:** `contracts-exoml/catalog.json` and `obligations.json` plus
  20 `.exocontract.json` files (12 root, eight sandbox) mix planner inputs,
  semantic assumptions, and unsupported obligations. Extract only necessary
  claims into checked overrides or explicit unsupported reasons. Clamp,
  Histprice/PriceOpt and `sandbox/ipc-framed` are profile handoffs (W07–W09).
  JSON ownership/invalidation, Vec alias/stride/realloc, Parsec borrowed
  descriptors/predicates, and Mune roots/retained callbacks must not disappear.
  Retain the distinct PrettyPrint companion and PrettySexpr boundaries,
  ElviScrape buffers/status/TLS callbacks, QUIC/HTTP3 client/view/network
  obligations and eight sandbox profiles' discovery and C archive limits.
  Do not turn the IPC companion into a public header or grant its profile to
  other sandbox declarations.
- **Exotract directory exceptions:** `contracts-exotract/catalog.json` has
  live recipes, `kernel-auto.json` drives registration/inspection, and
  `examples/{client,elvi-scrape,http3,microjs,mune}.c` exercise native APIs.
  Preserve or migrate the C fixtures. `kernels/mulib-cdd-accessors.h` is
  installed by `sdk/cmake/Subagent6.cmake` and implemented in
  `sdk/src/contracts/CddAccessors.c`; it is not a disposable declaration dump.
  Retain the header/API and checked CDD behavior, even if relocated.

## Consumers outside the compiler path

- **MuLIB registration:** `.githooks/pre-commit` invokes
  `tools/header_registration_hook.py`. That hook treats `MANIFEST.yaml` and
  `contracts-exotract/kernel-auto.json` as staged output together, through
  `tools/sync_inventory.py` and `tools/extend-kernelbind.py`. Root
  `CMakeLists.txt`, `Makefile` (`extend-kernelbind`) and `tools/check.py`
  exercise generation/freshness. Preserve staged-byte authority, index locking
  (including alternate indexes and linked worktrees), reservation records,
  alias collisions and no failed partial publication. A future hook must not
  require a private tool, network fetch, or silently write generated output
  with an unqualified target. W04 and the coordinator must decide how manifest
  registration and explicit Cephyr regeneration/CI interact. W04 owns
  freshness integration; the principal owns any root manifest/CMake edits.
  There is no tracked root `.github/workflows/` at the baseline: use the
  existing CMake/check entry points as the reference, not an assumed CI job.
- **C-only/raw archive users:** root `Makefile` targets
  `check-prettyprint-{exotract,contract,kernel-discovery}`,
  `tests/check_prettyprint.py`, `tests/prettyprint-kernel-discovery.on1x`, and
  `tests/finpred-tests/test_discovery.py` need migration or scoped retention.
  Update `MANIFEST.yaml` tracked-file records only through its owner. Preserve
  native PrettyPrint tests and the separately planned `check-prettyprint-on1x`.
- **SDK:** `sdk/src/Contract.cpp`, `sdk/src/contracts/ForeignAdmission.c`,
  `sdk/include/On1xSDK-{BindingGenerator,CDecl,Contract,ContractCheck,ContractReview,ExoLib,ExoML,Exotract,FFI,ForeignCallback,ForeignModule,ForeignResource,ForeignType,KernelBind,KernelDiscovery,Marshalling}.h`
  and their aggregate/package tests are distinct installed ABI surfaces.
  In particular `on1x_sdk_foreign_admit` accepts host review/boundary
  callbacks for structured candidates, including limited opaque resource and
  immutable string profiles. This is not the CLI's Ed25519 policy and is not
  retired by making the CLI use a new table. Decide ABI-compatible
  implementation/retention or explicit versioned change before removing these
  headers/symbols (principal and SDK owner). `stdlib/vmm` still uses
  `on1x-bind` + Chisel for its optional admission test; preserve its native
  bridge/configuration independently.
- **MCP and documentation:** `addons/mcp/core.py` allowlists and reads the
  legacy catalogues/sidecars and cites active KernelBind documentation;
  `addons/mcp/test_chatgpt.py` checks that output. Replace evidence paths and
  update the allowlist without turning the tool into an admission authority.
  Review `addons/CLI.md`, `docs/{architecture,capability-matrix,contracts,core-impl,ffi-strings,interfaces,kernel-*,memory-safety,numerics,roadmap,search-paths,source-migration}.md`,
  `stdlib/{README,VERIFICATION,mulib/README,container/{NATIVE,POD},daemon/README,on1xfile/README,prettyprint/README,vmm/README}.md`,
  `examples/contracts`, and the On1x book's chapters 93–97/119 and API/TeX
  references. Root manuals also reference kernel media, ElviScrape and
  PrettyDoc workflows. Preserve dated `COMPLETION_*`, benchmark results,
  `cephyr-ffi-module-verification.md` and independent test evidence as
  historical; point readers to current docs rather than rewriting old results.
- **Independent consumers:** MuLL's `mudsl/scripts/{generate_mulib,check_mulib_inventory}.py`
  and `mudsl/mull/bridge/Mulib/` independently enumerate canonical C headers
  and supply a different bridge. They are not a second On1x FFI implementation
  and must not be removed. Preserve public `kernels/mulib-*.h`, independent
  LibStone ExoML/Exotract, CDD itself, and foreign interfaces unrelated to the
  replaced standard-kernel path (e.g. trusted InlineC11/native extensions).

## Regression transfer and removal check

| Required behavior | Existing evidence to retain or port (test names/fixtures, not a pass claim) |
| --- | --- |
| Registration, reserve/alias, boundaries, fixtures | Root `mulib_kernelbind_inventory`/`mulib_kernelbind_generator`, `on1x.kernel_auto_catalog`/`on1x.kernel_auto_generator`, `tests/test_header_registration_hook.py`, `on1x/tests/test_{header_registration,bounded_kernel_inventory,bounded_kernel_discovery_sdk,cdd_discovery_sdk,kernel_auto_discovery_sdk,harness_inventory,static_kernel_registration}.py`; retain fail-closed discovery for providers/data-only/CDD/sandbox. |
| Raw plans/archives/inline wrappers/setup | `on1x.{exotract,kernel_paths,inline_wrappers_sdk,kernel_sandbox_sdk}`, `test_contracts.py`, `test_mulib.py`, `test_stdlib_project.py`, root PrettyPrint/FinPRED, archive fixtures under `contracts-exotract/examples/`; check flags, scope, path escape/ambiguity and stale archives. |
| Language, C ABI, review | `on1x.{kernel_imports,kernel_imports_sdk,kernel_review_sdk,kernel_dynlib,cephyr.clamp_abi,ffihandler.scalar}`, clamp oracle and C ABI fixtures, positive and negative canonical HIR/PPX/MEMGraph/ownership and native wrong-prototype tests. |
| Numerical and resource parity | `on1x.{kernel_history_sdk,integration.forecasting,kernel_ipc_hir,kernel_ipc_sdk,daemon_ipc}`, Histprice/Pricing C oracles, `runtime/on1x_kernel_ipc.*` and framed fixtures; retain unknown-effects limits, poisoning/error branches, scheduler constraints, sanitizer/cleanup and callback tests. |
| Other contracts and installed users | `on1x.{contracts,contracts_sdk,contract_strings,stdlib.vmm.admission,sdk.installed_consumer}`, `on1x/sdk/tests/contracts/`, CDD accessor/SDK export tests, project driver/build/fetch/lock and install scripts, MCP evidence tests. Preserve C JSON/Vec/Parsec/Mune semantic tests independently of sidecar formats. |

Discover actual CTest names for each configured variant with `ctest --test-dir
BUILD -N`. Compare optional gates and skip reasons before/after; a regex with
zero matching tests is not a pass. Before clean build and install, grep tracked
code, tests, tools, manifests, and docs for old paths, flags, import profile
and schema names. Classify remaining hits as active compatibility, retained
capability, independent consumer, or history. Inspect build/package contents
and test the installed driver without obsolete extraction dependencies. Do
not infer either dependency removal or ABI parity from a clean text search.
