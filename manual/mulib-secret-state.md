# Secret state

The host compare-and-swap callback owns interprocess atomicity and durability. A conflict or uncertain commit invalidates the local snapshot. Reconcile uncertain effects and reload authenticated state before issuing another operation. A callback test is not evidence of rollback protection. See [secret kernels](secret-kernels.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-secret-state.h`](../include/mulib-secret-state.h) |
| Implementation switch | `MULIB_IMPL_SECRET_STATE` |
| Repository-local includes | [mulib-secret-core](mulib-secret-core.md) |
| Complete example | [`examples/mulib-secret-kernels.c`](../examples/mulib-secret-kernels.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-secret-kernels.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
