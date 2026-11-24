# termscript-runtime — Bounded stack interpreter

Initialize a VM and run parsed or compiled programs in it. The stack,
bindings, modules and mode-aware key handlers persist across runs. Limits
bound work, calls, stack and allocated storage. Source programs are borrowed
by definitions and values until VM clear; the VM owns cached loaded modules.

The core does not inspect the environment, load a file, or write to a terminal
without an explicit host. See the [language manual](mulib-termscript.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-termscript-runtime.h`](../kernels/mulib-termscript-runtime.h) |
| Implementation switch | `MULIB_IMPL_TERMSCRIPT_RUNTIME` |
| Repository-local includes | [`kernels/detail/mulib-termscript-capsule.inc`](../kernels/detail/mulib-termscript-capsule.inc), [mulib-termscript-ast](mulib-termscript-ast.md) |
| Complete example | [`library/termscript/main.c`](../library/termscript/main.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels library/termscript/main.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
