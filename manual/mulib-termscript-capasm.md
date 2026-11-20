# Termscript lowering through CAPASM

The Termscript front end lowers a canonical executable AST to an
[XMacIR capsule](mulib-xmacir.md). [CAPASM](mulib-capasm.md) emits a C90
translation unit for it. Link that unit with a C11 build of the
Termscript runtime and its dependencies, not with a compiler or m4 at
runtime.

The exported entry takes a Termscript VM and an optional error pointer.
On invocation, the runtime checks the versioned payload and retains its
decoded data for values and bindings that survive the call. No Termscript
source is reparsed; execution remains bounded by the Termscript VM.
The generated code does not grant terminal, module, or shell capabilities.

This replaces the old Termscript-specific C emitter. `--compile ENTRY`
continues to write a C translation unit by default.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-termscript-capasm.h`](../include/mulib-termscript-capasm.h) |
| Implementation switch | `MULIB_IMPL_TERMSCRIPT_CAPASM` |
| Repository-local includes | [`include/detail/mulib-termscript-capsule.inc`](../include/detail/mulib-termscript-capsule.inc), [mulib-capasm](mulib-capasm.md), [mulib-strbuf](mulib-strbuf.md), [mulib-termscript-ast](mulib-termscript-ast.md) |
| Complete example | [`library/termscript/main.c`](../library/termscript/main.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude library/termscript/main.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
