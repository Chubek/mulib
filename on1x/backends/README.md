# On1x built-in backends

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

These adapters produce C11, canonical MIR text, CFG DOT, or evaluator output.
They are not native machine-code backends. MuLIB's native pipeline consumes a
different IR and emits assembly for an external assembler; no verified bridge
from On1x MIR exists, so SDK assembly/object/executable requests return
`ON1X_SDK_UNSUPPORTED`.

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
