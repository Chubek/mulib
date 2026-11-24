# Secret provider

Register only trusted callbacks and freeze the registry before use. V2 dispatch validates per-operation schema and bounds, and consumes required approval before invoking provider callbacks. An invalid result after a successful callback is an uncertain effect, not permission to retry. This API neither authenticates IPC peers nor sandboxes providers. See [secret kernels](secret-kernels.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-secret-provider.h`](../kernels/mulib-secret-provider.h) |
| Implementation switch | `MULIB_IMPL_SECRET_PROVIDER` |
| Repository-local includes | [mulib-secret-lease](mulib-secret-lease.md), [mulib-secret-policy](mulib-secret-policy.md) |
| Complete example | [`examples/mulib-secret-kernels.c`](../examples/mulib-secret-kernels.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/mulib-secret-kernels.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
