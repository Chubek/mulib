# mulib-art

[Manuals](README.md) · [DEX compiler](mulib-dexcc.md) · [JVM](mulib-jvm.md)

A small, verified **DEX integer-method VM** using ClickIR. This is not AOSP ART,
Android's framework, an APK runner, dex2oat, or a Java class library. The local
`standards/android` documents inform DEX layout, register conventions and runtime
semantics; AOSP's GC, object model, native artifact formats and tooling are not
reimplemented or implied by the module name.

## Integration

Define `MULIB_IMPL_ART` plus `DEXCC`, `BUFFER`, `AUTHCRYPTO`, `ADLER32`, `CLICKIR`,
`GSSAIR`, `MIR`, `JIT`, `DYNCALL`, `VMWEAVE`, `EGRAPH`, and `REWRITE` implementation
switches in one C11 translation unit. No switch is enabled implicitly. The
[complete example](../examples/modules/art.c) uses the explicit setup in
[`examples/android_runtime_setup.h`](../examples/android_runtime_setup.h).
On Unix link `-lm`; no Android SDK, JNI, external VM or compiler is required.

## Loading and compilation

`compile` accepts the verified normalized int-register method from
[DEXCC](mulib-dexcc.md). `load_code` accepts raw DEX instruction bytes and frame
register/argument counts. `load_dex` selects a class/name/parameter-count match
from a DEX035..040 image, checking its envelope hashes, every referenced pool
entry/bound, integer signature, static/direct definition, and selected code item.
Unrelated classes and methods are not verified. Debug attributes are not executed;
nonempty exception-handler tables are unsupported. There is no class initialization,
method dispatch, object heap, native invocation, monitor, GC or managed exception
handler support. Divide by zero is reported as a VM status.

All inputs are copied. Mutable registers lower to typed SSA with a synthetic
entry and PHIs through `mulib_clickir_from_native`; the resulting graph is actually
validated and lowered through ClickIR to executable MIR. `mulib_art_graph` exposes
that immutable graph for inspection. Java int wrapping uses explicit mask/sign
extension, not C signed overflow. Shifts mask the count to five bits, unsigned
right shift clears high bits, and `INT32_MIN/-1` wraps as Java requires. Division
and remainder by zero preserve the destination and report DIVIDE_BY_ZERO.

## Execution, JIT and AOT

`frame_init` copies int arguments into a zero-initialized `mulib_mir_frame_t`.
`run` supports bounded resume; budgets count **lowered MIR operations**, not DEX
bytecodes. STEP_LIMIT is resumable; FINISHED and arithmetic faults persist.
`invoke` is a convenience one-shot call whose output changes only on FINISHED.
Release frames with `mulib_mir_frame_free` before destroying their method.

`jit` explicitly compiles the validated executable image using MuLIB's baseline
native JIT. It emits real native control flow and calls shared instruction helpers;
it is not an optimizing machine-code compiler. Native execution is currently
Linux AMD64/SysV, with RW-to-RX (never RWX) memory. Other targets return UNSUPPORTED;
OS executable-memory denial returns HOST_ERROR. There is **no silent interpreter
fallback** for a failed JIT request. Newly initialized frames select the JIT image;
already-live interpreter frames continue to work. No automatic hotness/profiling
policy or on-device ARM64 JIT is provided.

`emit_c` generates portable C11 baseline AOT with direct control-flow labels and
the same runtime helpers. Compile the generated source yourself; MuLIB never
spawns a compiler. The generated `mulib_generated_NAME_program()` returns the
exact image for frame initialization; set `frame.remaining` before calling the
generated `mulib_generated_NAME(frame*)` function. This is not OAT/VDEX or ELF
emission. It preserves interpreter/JIT arithmetic and fault semantics.

## Ownership, limits and results

Method outputs start NULL; failed construction leaves them NULL. Methods own
their graphs, signatures and executable/JIT images. Borrowed views expire at
destroy. `destroy(&method)` is NULL-safe and zeros the pointer after success;
native unmap failure retains ownership. Independent frames may share immutable
methods; compilation/JIT/destroy must not race execution or each other.

`limit` is explicit and nonzero for useful work. It bounds every generated array,
source bytes, instruction/register products and dense CFG-squared analysis cells.
SSA materializes PHIs for each register/block, so this conservative implementation
is intended for small methods, not unbounded application images. OOM, resource
limits, invalid references/types, unsupported features, arithmetic errors and
output-writer errors remain distinguishable MIR statuses. Diagnostics refer to
the failing compilation stage, not a guaranteed DEX source map. C array bounds
and frame immutability remain caller obligations; this is not a security sandbox.

`tests/test_android_runtime.c` differentially checks all supported binary int
operations over signed/shift boundaries, faults, loops, resume and native JIT.
Allocation failures are enumerated in `tests/test_android_alloc.c`.
`tests/test_android_hostile.c` exercises randomized bytecode, truncated inputs
and rehashed DEX metadata mutations under sanitizer builds.
`tools/check_android.py` compiles and executes the generated C AOT independently.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-art.h`](../include/mulib-art.h) |
| Implementation switch | `MULIB_IMPL_ART` |
| Repository-local includes | [mulib-clickir](mulib-clickir.md), [mulib-dexcc](mulib-dexcc.md), [mulib-jit](mulib-jit.md) |
| Complete example | [`examples/modules/art.c`](../examples/modules/art.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/art.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
