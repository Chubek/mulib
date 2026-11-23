# `std.cephyr`: embedded C compiler for trusted tooling

```on1x
import std.cephyr as C;

fn main() {
  let (status, assembly, diagnostics) = C.emit_assembly("int answer(void){return 42;}");
  if status == 0 { println(assembly); } else { println(diagnostics); }
}
```

| Function | Return value |
| --- | --- |
| `check(source)` | `(status, diagnostics)` from `-fsyntax-only` |
| `emit_mir(source)` | `(status, archon_mir_text, diagnostics)` |
| `emit_assembly(source)` | `(status, amd64_assembly_text, diagnostics)` |

Each call compiles one independent, in-memory translation unit. Status is
an `i64`: `0` success, `1` compiler error, `2` invalid request/ABI mismatch,
`3` internal error (including unrepresentable results). Errors have empty
output and nonempty diagnostics. Input, output and diagnostics are NUL-free
On1x strings limited to 4 MiB each. Output and diagnostics are immutable
snapshots in the normal process-lifetime On1x text pool; its cumulative
limit and fatal allocation-failure behavior apply. Use from a single-threaded
On1x tool. The runtime text pool is not a concurrent allocator.

## Build and link

Use the [optional backend build recipe](../../BACKEND.md#0-source-baseline-revisions-and-the-exact-newly-added-work)
to configure a native AMD64 Linux build against the project-selected LibStone
(`4c35077601899944594737559534aafd9014f330`, Cephyr embedding ABI 1).
From the repository root, with the example above saved as `consumer.on1x`:

```sh
cmake --build build/A-enabled --target on1xc on1x_stdlib_cephyr
build/A-enabled/on1xc --emit-c consumer.on1x -o build/A-enabled/consumer.c
cc -std=c11 -Ion1x/runtime build/A-enabled/consumer.c on1x/runtime/on1x_runtime.c \
  build/A-enabled/stdlib/cephyr/libon1x_stdlib_cephyr.a \
  -Lbuild/A-prefix/lib -Wl,-rpath,"$PWD/build/A-prefix/lib" -lcephyr \
  -o build/A-enabled/consumer
build/A-enabled/consumer
ctest --test-dir build/A-enabled -R '^on1x\.stdlib\.cephyr$' --output-on-failure
```

The consuming executable needs the shared Cephyr library and its matching
native packages and the configured Cephyr/Archon resources. It does not link
or initialize CPython.
The companion embeds absolute resource and config paths. For deployment
configure `ON1X_STDLIB_CEPHYR_CONFIG` to the trusted installed
`share/on1x/cephyr-baseline.toml` and set `ON1X_CEPHYR_RESOURCE_ROOT` and
`ON1X_CEPHYR_ARCHON_ROOT` to the deployed resources. Keep them available at
runtime. With `ON1X_WITH_CEPHYR=OFF` no companion is built.

## Foreign boundary

`on1x_cephyr_analyze_v1` borrows its input synchronously, selects a fixed
operation (`0` check, `1` MIR, `2` assembly), copies representable result
text and destroys the Cephyr result. `on1x_cephyr_output_v1` and
`on1x_cephyr_diagnostics_v1` return the current call's snapshots.
The three declarations are raw externs and have no reviewed safe-import
contract. Syntax checking is not FFI verification. Source, transitive
includes, build resources and host environment must be trusted: the driver
can read files, load native backends, and use external tools. The baseline
picks `x86_64-linux-gnu` with Blackstone, SandSLAB and FactIR off and no QML passes;
the caller's `Cephyr.toml` and `CEPHYR_CONFIG` do not select driver options.
There is no module loading or invocation API.
