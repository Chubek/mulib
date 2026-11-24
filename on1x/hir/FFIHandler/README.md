# FFIHandler v0

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

The grammar also allows `inline fn` for forwarding to a static-inline function and zero-argument `fn`. These are tested in the standalone emitter, not new admitted source profiles. All parameters/results must be `i64` in v0. At most one inclusive ordered-parameter guard per function. The guard operands must be declared parameters. The provider and header must correspond to canonical MuLIB naming, not arbitrary paths; the existing build driver still decides the actual native dependency closure. Descriptor compilation refuses duplicate module names, member names and C symbols. No arbitrary C, actions, includes, pointers, resources, user-supplied trust or general effects are implemented. The compiler currently registers only `clamp.ffi` in the production CMake table; adding other source files alone does not admit them.

The declaration's `inline` marker is descriptive: the compiler emits a local forwarding function for either kind. C type compatibility is checked via `_Generic`; this is not a complete ABI or behavioral proof. The existing build driver still validates reviewed adapter identity and native evidence.

Tests: `ctest --test-dir BUILD -R '^on1x\.ffihandler\.scalar$' --output-on-failure` generates the real parser before execution as part of the normal frontend build. See the archive README for offline test limitations.
