# Secret policy

Apply rules to trusted subject identity and authenticated object metadata. An in-process approval flag is not client consent. Keep authorization and effect under a broker-controlled synchronization boundary. See [secret kernels](secret-kernels.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-secret-policy.h`](../include/mulib-secret-policy.h) |
| Implementation switch | `MULIB_IMPL_SECRET_POLICY` |
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
