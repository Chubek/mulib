# MuLIB vm-host

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/vm-host.c)

`register` copies a non-NULL function pointer, borrowed context, and exact arity
(0..1024), returning a stable index. `call` checks ID, argument count, and non-NULL
arguments for nonzero arity before invoking the callback. Callback receives
borrowed signed-64 arguments and a temporary result pointer.

On nonzero callback result, the temporary value is committed to caller output;
otherwise output stays unchanged. Callback side effects are retained. Context
lifetime belongs to the caller, and callbacks must not mutate their registry
during a call. No host calls happen implicitly in the bytecode interpreter.
Zero-initialize and `free`; registration amortized O(1), dispatch O(1).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-vm-host.h`](../include/mulib-vm-host.h) |
| Implementation switch | `MULIB_IMPL_VM_HOST` |
| Repository-local includes | None |
| Complete example | [`examples/modules/vm-host.c`](../examples/modules/vm-host.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/vm-host.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
