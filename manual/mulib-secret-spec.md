# Secret kind catalogue

The catalogue describes intended credential families and operations. It is not evidence of provider capability, permission, or interoperability. Missing operations must fail closed rather than being simulated. See [secret kernels](secret-kernels.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-secret-spec.h`](../include/mulib-secret-spec.h) |
| Implementation switch | `MULIB_IMPL_SECRET_SPEC` |
| Repository-local includes | [mulib-secret-policy](mulib-secret-policy.md) |
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
