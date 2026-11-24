# Secret JOSE

The fixed-header compact HS256 JWS primitive verifies the signature only. It is not a JWT claims verifier: the caller owns key selection, issuer, audience, time and replay checks. See [secret kernels](secret-kernels.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-secret-jose.h`](../kernels/mulib-secret-jose.h) |
| Implementation switch | `MULIB_IMPL_SECRET_JOSE` |
| Repository-local includes | [mulib-secret-codec](mulib-secret-codec.md), [mulib-secret-crypto](mulib-secret-crypto.md) |
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
