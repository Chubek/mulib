# Native reference extension

This directory contains the narrow trusted bridge and static module adapter for
the On1x-authored byte analyzer in
`on1x/sdk/examples/native/reference_analyzer.on1x`.

The host copies source bytes into bounded storage, grants capabilities
independently of the manifest request, and passes only 64-bit
generation/slot IDs plus fixed-width scalars to generated On1x. Every object
operation checks generation, kind, owner context, lifecycle state, revision and
permission. Diagnostic text is selected from a host-owned fixed catalogue and
copied with explicit lengths. Candidate bytes remain in host staging and are
copied to the published slot only after schema/revision validation succeeds.

The adapter is explicitly trusted and statically allowlisted. It is not
sandboxed and does not establish general FFI safety. It does not load names,
paths, contracts, or callbacks from the manifest. Discovery reads the JSON and
constant descriptor without invoking the extension.

## Lifecycle and limits

The host invokes `INITIALIZE`, `ANALYZE`, and `SHUTDOWN` in order. Initialization
stages copied metadata and compensates it on callback failure. Analysis has
separate deterministic work, diagnostic, source-byte, candidate-byte and
cancellation limits. Limit exhaustion is not a deadline. Every failure path
closes the borrowed snapshot and aborts candidate staging.

## Build

Build outside the source tree:

```sh
cmake -S on1x -B /tmp/opencode/on1x-sdk-native \
  -DBUILD_TESTING=ON -DON1X_BUILD_NATIVE_PPX=OFF
cmake --build /tmp/opencode/on1x-sdk-native \
  --target on1x_sdk_native_reference_host \
    on1x_sdk_native_stdlib_positive on1x_sdk_native_header_cpp --parallel 4
ctest --test-dir /tmp/opencode/on1x-sdk-native --output-on-failure \
  -R '^on1x\.sdk\.native\.'
```

`on1xc` emits C11 first. The build renames only the generated executable
`main`, links the generated module to the bridge and canonical runtime, and
calls its scalar entry through `module_adapter.c`.

## ABI 2 migration

When the public ABI 2 host table is available, replace `_Trusted.on1x` and this
bridge with reviewed generated bindings for allocator, handle, diagnostic,
budget, cancellation and candidate operations. The required signatures and
cross-owner requests are recorded in
`sdk/catalogue/agent-reports/subagent-8.json`. Until then, DSO/worker loading,
arbitrary extension registration and structured compiler snapshots reject as
unsupported.
