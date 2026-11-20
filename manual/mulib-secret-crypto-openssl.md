# OpenSSL secret crypto

Optional EVP adapter. Define its implementation switch only in a build with OpenSSL development headers and link `libcrypto`. Declarations and the native example below do not require OpenSSL. See [secret kernels](secret-kernels.md#build-and-verification) for the opt-in test.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-secret-crypto-openssl.h`](../include/mulib-secret-crypto-openssl.h) |
| Implementation switch | `MULIB_IMPL_SECRET_CRYPTO_OPENSSL` |
| Repository-local includes | [mulib-secret-crypto](mulib-secret-crypto.md) |
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
