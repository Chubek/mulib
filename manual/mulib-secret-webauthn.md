# Secret WebAuthn

Build basic assertion bytes and signing inputs. This is not an authenticator or an origin verifier. Only the host can establish user presence and verification, credential ownership, and RP and challenge bindings. See [secret kernels](secret-kernels.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-secret-webauthn.h`](../kernels/mulib-secret-webauthn.h) |
| Implementation switch | `MULIB_IMPL_SECRET_WEBAUTHN` |
| Repository-local includes | [mulib-secret-crypto](mulib-secret-crypto.md) |
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
