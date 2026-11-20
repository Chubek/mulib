# ed25519 — Ed25519 through OpenSSL

Define `MULIB_IMPL_ED25519` in one C translation unit and link OpenSSL
`libcrypto` (1.1.1 or newer). Declarations do not require OpenSSL headers.
Use a raw 32-byte private seed or raw 32-byte public key, not a PEM file or SSH
key blob. Sign and verify the original message, not a precomputed digest;
signatures are 64 bytes. Private seed storage and cleanup are caller-owned.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ed25519.h`](../include/mulib-ed25519.h) |
| Implementation switch | `MULIB_IMPL_ED25519` |
| Repository-local includes | None |
| Complete example | [`examples/ssh/kernels.c`](../examples/ssh/kernels.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/ssh/kernels.c -pthread -lm -ldl -lcrypto -o mulib-example
```
<!-- /manual-reference -->
