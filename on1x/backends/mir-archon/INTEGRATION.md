# Native backend integration

The original package required the integration work below. It is now wired
into the compiler as an explicit opt-in native backend, with separate trusted
SDK admission and standalone paths.

## 1. CMake wiring (implemented)

`on1x/backends/CMakeLists.txt` includes the guarded opt-in subdirectory after
creating the existing built-in adapters:

```cmake
option(ON1X_BUILD_MIR_ARCHON "Build Archon native MIR backend" OFF)
if(ON1X_BUILD_MIR_ARCHON)
  add_subdirectory(mir-archon)
  if(TARGET on1x_backend_mir_archon)
    list(APPEND _on1x_backend_artifacts on1x_backend_mir_archon)
  endif()
endif()
```

Build standalone by enabling `ON1X_BUILD_MIR_ARCHON` and setting
`ON1X_ARCHON_TARGET_TRIPLE=x86_64-unknown-linux-gnu` (or the true canonical
triple for the generated native target). For provider registration also set
`ON1X_ARCHON_ENABLE_SDK_ADAPTER=ON`. Note that the parent On1x project
currently declares `on1x_runtime` **after** entering `backends`; this folder's
CMake deliberately allows the target to be created later.

## 2. Manifest registrars (implemented)

Both the portable and Yellowstone manifests include the native registrar.
The portable entry and generated C++ entry are guarded by
`ON1X_WITH_MIR_ARCHON`, defined only when the SDK adapter target is linked:

```text
# on1x/backends/BuiltinRegistrars.inc
ON1X_BUILTIN_REGISTRAR("mir-archon", on1x_builtin_mir_archon_register)
```

```lua
-- on1x/backends/link/backends.lua
qld.backend("mir-archon", "on1x_builtin_mir_archon_register")
```

The strict `registrars` table in
`on1x/backends/link/GenerateRegistrars.cpp` includes that exact pair. It is a
closed allowlist; adding only the Lua entry makes generation fail.

## 3. Trusted native-artifact admission (implemented)

`on1x/sdk/src/Backend.cpp` admits `ASSEMBLY` only for explicitly named,
compiler-linked trusted native providers receiving verified MIR and matching
an immutable target. Public external provider registration still rejects native
authority; SDK `OBJECT` and `EXECUTABLE` production remain unsupported.
Target/schema/feature matching, cancellation, budgets, staged bytes and atomic
publication remain enforced. See [SDK security](../../sdk/SECURITY.md) and
`on1x.sdk.native_backend_{v2,artifact}`.

## 4. Target-aware driver invocation (implemented)

`on1xc --sdk-emit mir-archon --sdk-target x86_64-unknown-linux-gnu INPUT`
uses `run_native`/`run_mir_native` and target-aware
`on1x_sdk_backend_produce()`. It supplies verified private MIR with its exact
schema, a validated immutable target and an assembly request. `--optimize-ir`,
`--native-pic` and `--native-no-entry` are supported. Plugin selection is
rejected on this path. Existing MIR and legacy SDK modes retain their behavior.

The standalone `on1x-archon-mir` driver already supports the same backend
without an SDK policy change and is the preferred initial integration test.

## 5. Executable linkage (implemented)

`tools/on1x-native-build.py` performs explicit source-to-native compilation,
assembly and linkage with the scalar ABI, On1x runtime and platform libraries.
It validates toolchain targets and ELF artifacts, bounds subprocess resources
and publishes atomically. Provider callbacks never execute external tools.
See [driver usage and tests](DRIVER.md).

## 6. Release gate

AMD64 native differential, exact trap, ABI, cancellation and resource tests
are executable CTests; see [measured evidence](tests/VERIFICATION.md).
Other-target execution and a fully instrumented entire toolchain remain
unqualified. Native emission remains opt-in and limited to the admitted MIR
subset.

Before enabling native output by default, run differential tests against
`mir::evaluate()` and `mir::emit_c()`, including loop-carried tuples,
float/unsigned arithmetic, exceptions/traps, executable ABI validation and
malformed MIR. Use sanitizer builds and target-native/QEMU execution; an
assembly string test alone is not evidence of behavioral equivalence.
