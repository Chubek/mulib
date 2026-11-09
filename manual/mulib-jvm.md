# mulib-jvm

[Manuals](README.md) · [ART backend](mulib-art.md)

A focused JVM **static integer-method** frontend using the strengthened ClickIR
pipeline. It is not a complete JVM/Java SE implementation. It shares the verified
int arithmetic, resumable frames and real baseline JIT/C AOT machinery with
[mulib-art](mulib-art.md), rather than maintaining two inconsistent runtimes.

## Integration and loading

Define `MULIB_IMPL_JVM` and every [ART implementation dependency](mulib-art.md),
in one C11 translation unit. Link `-lm` on Unix. The
[standalone example](../examples/modules/jvm.c) and its
[setup header](../examples/android_runtime_setup.h) list all switches. Default
builds need no Java installation, SDK, JNI, third-party VM or compiler process.

`mulib_jvm_load_code` takes borrowed Code bytes, max_locals/max_stack and parameter
count. The method signature is static `(I...I)I`; at most 255 int parameters.
Optional constant-pool arrays are indexed by JVM slot and permit tag-3 Integer
entries for ldc/ldc_w. Bytes/tables are not retained after compilation.

Supported instructions:

* nop; iconst_m1..5, bipush, sipush, integer ldc/ldc_w;
* iload/istore and their _0..3 forms; iinc; wide iload/istore/iinc;
* pop, dup, swap;
* iadd/isub/imul/idiv/irem/ineg, ishl/ishr/iushr/iand/ior/ixor;
* ifeq/ne/lt/ge/gt/le, if_icmpeq/ne/lt/ge/gt/le, goto/goto_w and ireturn.

Objects, arrays, fields, calls, switches, jsr/ret, exceptions, monitors, long/float/
double values, invokedynamic and native methods are unsupported. Every unsupported
opcode is rejected, including in unreachable bytes. The verifier computes stack
heights over all edges, rejects under/overflow and inconsistent joins, validates
local indices and all-path definite assignment, and requires every instruction
to be reachable. ireturn requires exactly one int on the stack in this profile.
Backward branches/loops and merges are supported; termination is not required.

`load_class` structurally parses major-version 45..71 class files and selects one
matching method name/descriptor. Constant pools, interfaces, fields and attributes
are walked with checked lengths; unrelated code need not be in the executable
subset. Selected methods must be static, nonsynchronized, nonnative, nonabstract,
with one Code attribute and no handlers. StackMapTable is not trusted or used:
the focused integer verifier recomputes its own stack/local facts. This API is
not a full JVMS class verifier, hierarchy loader, linker or Java boot process.
It never runs class initializers. Array/instance/reference return types are not
accepted under the integer profile.

## Compilation and execution

Stack operations become typed mutable virtual registers, then verified DEXCC
register records, then actual ClickIR SSA/PHIs, then safe executable MIR.
`mulib_jvm_graph` exposes the actual graph. Java int wrapping, five-bit shift
masking, signed/unsigned right shifts, MIN/-1 division and zero faults are shared
with ART; arithmetic is never accidentally promoted to Java long semantics.

`frame_init`, `run`, `invoke`, `jit`, `emit_c`, and `destroy` have the same
[ownership, budgets, concurrency, JIT targets and AOT contract](mulib-art.md).
In particular native JIT is explicitly requested, Linux AMD64 only, with W^X;
AOT emits C11 source to a writer and does not spawn a compiler. Existing frames
must outlive neither their method nor its program metadata. Result output changes
only on FINISHED. Frame budgets count lowered MIR operations, not JVM opcodes.

## Bounds, diagnostics and evidence

All method owners begin NULL and construction failure is atomic. Source lengths,
pool counts, generated registers/records, register*block products and CFG-squared
analysis must fit `limit`; zero is not unlimited. Resource-limit and allocation
failure results are distinct. No method input can request arbitrary host pointers
or external libraries. Readable C buffers and unmodified frame/program descriptors
remain the caller's responsibility; execution is not a hostile-code sandbox.
Diagnostics identify the failing frontend/IR stage; no complete JVM debug/source
mapping is promised. Writer failure can leave partial AOT text.

`tests/test_android_runtime.c` checks branches, loop-carried PHIs, stack errors,
uninitialized locals, all supported binary arithmetic boundaries, interpreter/JIT
parity and resumption. `tools/check_android.py` generates an original valid Java 5
class fixture, loads it and executes its loop, tests every truncated prefix, and
compiles/runs baseline AOT. It also uses javap when installed. Allocation failures
are exercised in `tests/test_android_alloc.c`. These are focused-profile checks,
not Java compatibility certification or on-device ART testing.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-jvm.h`](../include/mulib-jvm.h) |
| Implementation switch | `MULIB_IMPL_JVM` |
| Repository-local includes | [mulib-art](mulib-art.md) |
| Complete example | [`examples/modules/jvm.c`](../examples/modules/jvm.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/jvm.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
