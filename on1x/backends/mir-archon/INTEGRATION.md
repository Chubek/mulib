# Required upstream integration (outside this directory)

This folder is self-contained as a source package; it does **not** silently
change On1x's security policy. These changes are necessary for the optional
compiler-linked SDK 2 provider to become publicly selectable.

## 1. CMake wiring

In `on1x/backends/CMakeLists.txt`, after the existing `foreach()` creating
built-in adapters, add a guarded opt-in subdirectory:

```cmake
option(ON1X_BUILD_MIR_ARCHON "Build Archon native MIR backend" OFF)
if(ON1X_BUILD_MIR_ARCHON)
  add_subdirectory(mir-archon)
  if(TARGET on1x_backend_mir_archon)
    list(APPEND _on1x_backend_artifacts on1x_backend_mir_archon)
  endif()
endif()
```

Build standalone by enabling `ON1X_BUILD_MIR_ARCHON`. For provider registration
also set `ON1X_ARCHON_ENABLE_SDK_ADAPTER=ON` and
`ON1X_ARCHON_TARGET_TRIPLE=x86_64-unknown-linux-gnu` (or the true canonical
triple for the generated native target). Note that the parent On1x project
currently declares `on1x_runtime` **after** entering `backends`; this folder's
CMake deliberately allows the target to be created later.

## 2. Manifest registrars

If enabling the built-in SDK adapter, append the following to both the
portable and Yellowstone manifests:

```text
# on1x/backends/BuiltinRegistrars.inc
ON1X_BUILTIN_REGISTRAR("mir-archon", on1x_builtin_mir_archon_register)
```

```lua
-- on1x/backends/link/backends.lua
qld.backend("mir-archon", "on1x_builtin_mir_archon_register")
```

Extend the strict `registrars` table in
`on1x/backends/link/GenerateRegistrars.cpp` with that exact pair. It is a
closed allowlist; adding only the Lua entry makes generation fail.

## 3. Do not weaken native-artifact admission indiscriminately

`on1x/sdk/src/Backend.cpp` currently refuses
`ON1X_SDK_ARTIFACT_ASSEMBLY/OBJECT/EXECUTABLE` at provider registration and
`on1x_sdk_backend_produce`. It must be changed **only for trusted, verified,
explicitly selected, target-matched native providers**. External plugin
providers must remain barred from claiming native artifact authority merely
by choosing an output enum value. Retain the existing target/schema/feature
matching, cancellation, budgets, byte staging and atomic artifact publication.

A sound change should relocate the rejection until after capability selection,
then test `selected.trusted`, target descriptor, input verification and host
admission policy. Do not simply remove the rejection or set
`ON1X_SDK_BACKEND_TARGET_INDEPENDENT`. Update the SDK's published contract and
its positive/negative native-artifact tests at the same time.

## 4. Target-aware driver invocation

`on1x::backends::run_mir()` currently selects providers only when they declare
`ON1X_SDK_BACKEND_TARGET_INDEPENDENT`, and the public `--sdk-emit` driver calls
legacy `on1x_sdk_backend_run()`, not target-aware
`on1x_sdk_backend_produce()`. Neither path can invoke this provider as written.

Add a native emit mode to `on1xc`, or an explicit SDK 2 producer path with an
immutable, validated `On1xSDK_Target`, an assembly request and a selected
provider name `mir-archon`. The compiler should use a trusted private MIR view
or verified serialization and transfer artifacts through the existing staging
sink. The new mode must fail closed on target mismatch and should not change
existing `--emit-ir-c`, `--emit-ir`, `--ir-cfg` or `--run-ir` behavior.

The standalone `on1x-archon-mir` driver already supports the same backend
without an SDK policy change and is the preferred initial integration test.

## 5. Executable linkage

Assembly is not a complete program. Provide a compiler driver / build-tool
step which assembles and links it with `on1x_archon_scalar_abi`, `on1x_runtime`,
any required On1x runtime platform dependencies, and the platform C runtime.
Use the generated backend manifest to confirm the correct target and toolchain.
Do not execute untrusted assembly in the compiler process.

## 6. Release gate

Before enabling native output by default, run differential tests against
`mir::evaluate()` and `mir::emit_c()`, including loop-carried tuples,
float/unsigned arithmetic, exceptions/traps, executable ABI validation and
malformed MIR. Use sanitizer builds and target-native/QEMU execution; an
assembly string test alone is not evidence of behavioral equivalence.
