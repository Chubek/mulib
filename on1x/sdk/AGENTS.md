# SDK 2 implementation rules

This file augments the repository and `on1x/AGENTS.md` instructions.

## Frozen shared contracts

All SDK workers must read `ARCHITECTURE.md`, `ABI.md`, `COMPATIBILITY.md`,
`SECURITY.md`, `EXTENSIONS.md`, `IMPLEMENTATION_INVENTORY.json`, and
`CAPABILITY_MATRIX.json` before editing. Do not introduce a competing registry,
allocator, handle encoding, diagnostic representation, type system, IR identity,
serializer, plugin loader, or capability authority.

The authoritative compiler always performs mandatory semantic, ownership,
safety, foreign-admission, and IR verification. External constructions are
candidates. Unsupported functionality rejects explicitly.

## Principal-owned shared files

Only the principal integration agent edits:

- `on1x/CMakeLists.txt`;
- `on1x/src/cli.cpp`;
- `on1x/sdk/CMakeLists.txt`;
- `on1x/sdk/include/On1xSDK.h`;
- the nine top-level SDK architecture/report files;
- `CAPABILITY_MATRIX.json` and `IMPLEMENTATION_INVENTORY.json`;
- cross-subsystem integration tests.

Workers add build logic only to their own
`on1x/sdk/cmake/SubagentN.cmake` and
`on1x/sdk/cmake/IntegrationN.cmake` hooks. They add a machine-readable report at
`on1x/sdk/catalogue/agent-reports/subagent-N.json`.

## Exclusive ownership

### Subagent 1 — core/plugins

- Existing: `On1xSDK-Types.h`, `-Registry.h`, `-Diagnostic.h`, `-ExecEnv.h`;
  `Registry.cpp`, `RegistryInternal.hpp`, `MetadataInternal.hpp`,
  `PluginHost*`, `PluginWorker.cpp`, `PluginProtocol.hpp`, `PluginKernels.c`.
- New: core/plugin/capability/catalogue headers; `sdk/src/core`,
  `sdk/src/plugin`, `sdk/src/catalogue`, `sdk/protocol`, core schemas and tests.

### Subagent 2 — compiler/frontend

- New compiler/frontend public headers and `sdk/src/compiler`,
  `sdk/src/frontend`, frontend schemas/examples/tests.
- Canonical frontend extraction files only when required:
  `on1x/src/{source,lexer,lexer_fallback,parser,loader,ppx,ppx_ast,ppx_schema,macros,macro_rules,staging}.cpp`
  and corresponding public `on1x/include/on1x` headers.
- Do not edit type/MIR/pass/backend ownership files.

### Subagent 3 — types/semantics/IR

- New type/semantic/HIR/MIR public headers and `sdk/src/types`, `sdk/src/ir`.
- Canonical representation files:
  `on1x/include/on1x/{ast,type,infer,nominal,cliche,inherent,safety,type_safety}.hpp`,
  matching `on1x/src` files, and `on1x/include/on1x/backend/mir.hpp`,
  `on1x/src/backend/{mir,lower,serialize}.cpp`.
- Type/HIR/MIR schemas and focused tests.

### Subagent 4 — analyses/transformations

- Existing `On1xSDK-Analyzer.h`, `-Transformer.h`, `Analyzer.cpp`,
  `Transformer.cpp`, `FacetInternal.hpp`.
- New analysis/transformation headers and `sdk/src/analysis`,
  `sdk/src/transformation`, analyzer/optimizer directories and tests.
- Canonical `on1x/include/on1x/{analysis}.hpp`,
  `on1x/src/analysis.cpp`, and
  `on1x/include/on1x/backend/passes.hpp`,
  `on1x/src/backend/passes.cpp`.

### Subagent 5 — backends/toolchains

- Existing `On1xSDK-Backend.h`, `-Toolchain.h`, `Backend.cpp`,
  `Toolchain.cpp`, and `on1x/backends/**`.
- New backend/target/machine/toolchain headers and `sdk/src/backend`,
  `sdk/src/toolchain`, `on1x/toolchains`, backend tests/examples.
- Do not change canonical MIR structures owned by Subagent 3.

### Subagent 6 — FFI/contracts/runtime SDK

- Existing `On1xSDK-Contract.h`, `Contract.cpp`;
  `on1x/contracts-exoml`, `on1x/contracts-exotract`,
  contract/binding tools, and canonical `on1x/runtime/**`.
- New FFI/contract headers, `sdk/src/contracts`, and all
  `on1x/runtime-sdk/**`.
- Respect exact ABI/review gates and one-TU MuLIB planning.

### Subagent 7 — projects/tooling/bindings

- Existing `On1xSDK-CLIOpt.h`, `-Module.h`, `CLIOpt.cpp`, `Module.cpp`.
- New project/tooling public headers; `sdk/src/toolchain` project-side files,
  `sdk/tools`, `sdk/bindings`, `sdk/cmake` packaging files, project tests.
- Existing project/build scripts, formatter and `on1x/addons/lsp/**`.
- Do not edit backend toolchain files owned by Subagent 5; use
  `sdk/src/project` for project services.

### Subagent 8 — On1x-native SDK

- All `on1x/stdlib/sdk/**`.
- Native SDK examples/tests under `sdk/examples/native`,
  `sdk/tests/native`, and native extension sources in dedicated new directories.
- Follow `on1x/stdlib/AGENTS.md`; use reviewed contracts or explicitly trusted
  adapters and never expose raw pointers as safe values.

## Cross-owner changes

Do not edit another owner's file. Record a concrete request in the agent report
with required API, reason, and proposed signature. The principal resolves it
during integration. Minimal generated fixtures belong under the owner's test
directory.

## Required work product

Implement executable functionality, not declarations alone. Add argument
validation, ownership documentation, finite limits, structured errors,
positive/negative/integration tests, and truthful unsupported paths. Build in an
external directory. Commit intended files, report the commit SHA and exact test
commands/results, and do not commit build artifacts.
