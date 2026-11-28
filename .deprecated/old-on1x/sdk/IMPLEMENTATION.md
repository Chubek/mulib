# SDK 2 implementation inventory and plan

## Inspected baseline

- MuLIB repository: `6a145cbf09efcac76f387f6b664fc2257bac93aa`
- LibStone checkout: `3399047f6c6328b8f07788795c610699e66c060a`
- Platform: Linux x86-64, kernel 6.18.52
- GCC 16.2.1, Clang 22.1.8, CMake 4.4.3

Existing executable SDK facilities are ABI-v1 registration with rollback,
source analyzers/transformers, serialized verified-MIR analyzers/backends,
metadata registries, five built-in backend adapters, installed C/C++ consumers,
and a Linux x86-64 isolated DSO worker using MuLIB namespace, seccomp and framed
IPC kernels.

Existing reusable compiler facilities include the canonical scanner/parser,
ASDL/PPX schema, module loader, type inference, nominal/cliche/inherent
semantics, staging, safety checks, C11 emitter, scalar MIR verifier/serializer/
evaluator/emitter, transactional scalar passes, diagnostics, formatter, project
tools, contract extraction/admission, kernel planner, and AOT runtime.

Reusable MuLIB facilities include CDD catalogues/planning, sandbox/process/IPC,
shared memory, graphs, pass management, e-graphs, Mune, channels/tasks, assembler,
object-file, linker, and register-allocation kernels. They must be used through
their actual contracts and one implementation translation unit.

Reusable LibStone facilities include Chisel, Syren, Wetstone, Nerotypical,
Novostone/CCDB/Datalog, ExoML/Exotract, Plaid's documented partial concurrency
toolkit, Prolific's observational profiler, Yellowstone linkage, and the
available target/toolchain components. Optional availability never becomes a
false SDK capability.

The machine-readable detail is in
[`IMPLEMENTATION_INVENTORY.json`](IMPLEMENTATION_INVENTORY.json) and
[`CAPABILITY_MATRIX.json`](CAPABILITY_MATRIX.json).

## Integration stages

1. Core ABI 2 and compatibility adapter.
2. Compiler sessions and immutable structured representations.
3. Revision-aware analysis, transformation, backend, contract, project and
   runtime services.
4. Explicit `on1xc` and `on1x-sdk` integration.
5. On1x-native bindings and extensions.
6. Cross-subsystem, sanitizer, security, installation and relocation validation.

All eight worker slices are integrated. Their commit IDs and reports are
indexed in the machine-readable inventory. The aggregate keeps the canonical
compiler and scanner as CMake implementation dependencies, rather than
reimplementing them or exposing compiler C++ layouts through the C API.
`on1xc sdk` dispatches the existing data-only tool; the legacy compiler hooks
remain separate and explicit. See [verification](VERIFICATION.md) for tested
profiles and the [capability matrix](CAPABILITY_MATRIX.json) for exclusions.
