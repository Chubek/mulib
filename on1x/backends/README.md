# On1x built-in backends

In source builds, [On1x HIR](../hir/README.md) runs high-level optimizations
and lowers to verified MIR before dispatch to a `mir-*` backend. Direct C11
continues to consume checked HIR as the reference path. Serialized MIR starts
at MIR verification and does not run HIR passes.

See the [backend phase audit](../docs/backend-phase-audit.md) for actual
driver routes, regression commands, and remaining coverage gaps.

In an SDK-enabled compiler, `on1xc --backend-capabilities` reports the
[linked provider descriptors](BuiltinCapabilities.md). It does not check a
program, locate toolchains, or replay the
[release evidence](../docs/release-qualification.md).

In SDK-enabled builds, the compiler dispatches these existing modes through
the extension SDK registry and artifact sink:

| Mode | Backend | Input |
|---|---|---|
| `--emit-c` | `c11/` | Checked HIR |
| `--emit-ir-c` | `mir-c11/` | Verified MIR |
| `--run-ir` | `mir-eval/` | Verified MIR |
| `--emit-ir` | `mir-text/` | Verified MIR |
| `--ir-cfg` | `mir-cfg/` | Verified MIR |

Each adapter is a separate static library. It calls the canonical compiler
implementation, not a second emitter or evaluator. Built-ins are trusted and
see the already-checked compiler objects through private, synchronous views.
External plugins do not get these views. The built-in registry is separate
from external discovery; `ON1X_PLUGIN_PATH` cannot replace the default C11
backend. The SDK is not linked into emitted On1x programs.

The adapters register SDK 2 capability records as well as retaining their
ABI-v1 invocation behavior. Each record states its exact input phase/schema,
artifact kind, target independence, and determinism claim. SDK 2 selection
rejects schema, target, feature and output-kind mismatches before invoking a
callback, stages complete bytes privately, and fails on equal best-ranked
providers. The host remains the only artifact publication authority.

The baseline adapters above produce C11, canonical MIR text, CFG DOT, or
evaluator output. Two additional executable products have narrower admission
profiles:

- [VMWeave](mir-vm/README.md) emits verified `.o1vm` bytecode for the
  standalone `on1x-vm` runner or in-process guest execution.
- [Archon](mir-archon/README.md) is opt-in and emits native assembly from its
  admitted verified MIR subset. The tested Linux AMD64 SysV toolchain uses an
  assembler and linker to produce an executable. The SDK adapter requires
  explicit target configuration and trusted native artifact admission.

[Cephyr](../docs/cephyr-ffi-module-verification.md) is a distinct opt-in route
from checked HIR through emitted C11 to a Linux AMD64 object. None of these
products silently falls back to a different backend on an unsupported request.

`ON1X_BUILD_EXTENSION_SDK=OFF` retains direct dispatch to the same compiler
routines. The MIR optimization and optional analysis/reporting options
continue to run in the compiler before SDK backend dispatch.

To use the LibStone build-host linker manifest, configure
`-DON1X_USE_YELLOWSTONE_LINK=ON` (the default when a compatible native
checkout or an explicit host generator is available). Yellowstone evaluates
[`link/backends.lua`](link/backends.lua) to produce the registrar map that
selects symbols from the adapter archives. The target C++ compiler and linker
then compile the map and link the archives into `on1xc`; Yellowstone is not
linked into the driver. Cross builds can use a native generator via
`ON1X_YELLOWSTONE_HOST_GENERATOR`. See the [link build guide](link/README.md)
for the native generator and the portability limits.
