# sshcrypto — SSH key derivation and record protection

Define `MULIB_IMPL_SSHCRYPTO` and link OpenSSL `libcrypto` for SHA-256. Use
`mulib_ssh_derive` for RFC 4253 derivation from the Curve25519 shared secret,
current exchange hash and original session ID. Each AES-128-GCM direction owns
its state and nonce; never duplicate a direction to continue using both copies.
The visible four-byte packet length is authenticated as associated data.
Close and clear the session on a failed AEAD operation or limit exhaustion.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-sshcrypto.h`](../kernels/mulib-sshcrypto.h) |
| Implementation switch | `MULIB_IMPL_SSHCRYPTO` |
| Repository-local includes | [mulib-aes-gcm](mulib-aes-gcm.md), [mulib-sshpacket](mulib-sshpacket.md) |
| Complete example | [`examples/ssh/kernels.c`](../examples/ssh/kernels.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/ssh/kernels.c -pthread -lm -ldl -lcrypto -o mulib-example
```
<!-- /manual-reference -->
