# Native secret crypto

Adapts MuLIB entropy, SHA, HMAC, PBKDF2-SHA256 and AES-128-GCM. No external libraries are required by the adapter. AES-256-GCM, XChaCha20-Poly1305 and Argon2id are not available through it. See [secret kernels](secret-kernels.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-secret-crypto-native.h`](../include/mulib-secret-crypto-native.h) |
| Implementation switch | `MULIB_IMPL_SECRET_CRYPTO_NATIVE` |
| Repository-local includes | [mulib-aes-gcm](mulib-aes-gcm.md), [mulib-authcrypto](mulib-authcrypto.md), [mulib-entropy](mulib-entropy.md), [mulib-hkdf](mulib-hkdf.md), [mulib-secret-crypto](mulib-secret-crypto.md), [mulib-sha](mulib-sha.md) |
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
