# FFIHandler

FFIHandler validates target-qualified [MuLIB catalogues](../../stdffi/README.md)
and provides compiler-owned descriptors to checked HIR and the direct C11
backend. An executable descriptor still needs exact ABI checks, linkage, and
independent unsafe or reviewed-safe admission. Discovery reports never grant
that admission.

- [Descriptor format](INTERFACE.md) and [Cephyr analysis](../../docs/ffi-cephyr-evidence.md)
- [Unsafe framed IPC resources](../../docs/kernel-framed.md) and
  [MEMGraph lifetime and placement](../MEMGraph/RESOURCES.md)
- [Foreign status conditions](../../docs/ffi-foreign-conditions.md) and
  [taint policy](../../docs/ffi-taint.md)
- Checked C11 foreign calls on [AArch64](../../docs/ffi-aarch64.md) and
  [RV64IM](../../docs/ffi-rv64.md)

## Legacy v1 seed format

`ffi 1` files describe bounded scalar foreign calls; no DSL rule authorizes a safe call. The generated immutable table is consumed by the frontend and direct C11 backend. No LLSynth runtime is required in emitted applications.

```text
ffi 1;
foreign mulib.clamp {
    provider mulib "clamp";
    header "mulib-clamp.h";
    fn add_i64(a: i64, b: i64) -> i64 = c::mulib_clamp_add_i64;
    fn i64(value: i64, low: i64, high: i64) -> i64
        = c::mulib_clamp_i64 requires low <= high;
}
```

The grammar also allows `inline fn` for forwarding to a static-inline function and zero-argument `fn`. These are tested in the standalone emitter, not new admitted source profiles. All parameters/results must be `i64` in this legacy format. At most one inclusive ordered-parameter guard per function is permitted. The guard operands must be declared parameters. The provider and header must correspond to canonical MuLIB naming, not arbitrary paths; the existing build driver still decides the actual native dependency closure. Descriptor compilation refuses duplicate module names, member names and C symbols. The production registry uses the target-qualified v2 catalogue; adding a descriptor file alone does not admit native calls.

The declaration's `inline` marker is descriptive: the compiler emits a local forwarding function for either kind. C type compatibility is checked via `_Generic`; this is not a complete ABI or behavioral proof. The existing build driver still validates reviewed adapter identity and native evidence.

Tests: Build the normal frontend to generate the real LLSynth parser, then
run `ctest --test-dir BUILD -R '^on1x\.ffihandler\.' --output-on-failure`.
