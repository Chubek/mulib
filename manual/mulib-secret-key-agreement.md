# Secret key agreement

X25519 and transcript-derived HKDF output are building blocks, not a channel protocol. The caller must authenticate both roles and bind their identities and the entire protocol transcript. See [secret kernels](secret-kernels.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-secret-key-agreement.h`](../kernels/mulib-secret-key-agreement.h) |
| Implementation switch | `MULIB_IMPL_SECRET_KEY_AGREEMENT` |
| Repository-local includes | [mulib-secret-crypto](mulib-secret-crypto.md), [mulib-x25519](mulib-x25519.md) |
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
