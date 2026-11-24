# sshkex — SSH Curve25519 key exchange

This layer handles proposals and the `curve25519-sha256` exchange payloads.
The transcript uses exact identification text without CRLF, raw KEXINIT
payloads (including message number), and the canonical positive mpint encoding
of the shared secret. Do not reconstruct the peer's proposal or erase its
identification comment before hashing. Use [transport](mulib-sshtransport.md)
to drive the exchange and verify a trusted host key pin.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-sshkex.h`](../kernels/mulib-sshkex.h) |
| Implementation switch | `MULIB_IMPL_SSHKEX` |
| Repository-local includes | [mulib-ed25519](mulib-ed25519.md), [mulib-sshcrypto](mulib-sshcrypto.md), [mulib-sshkey](mulib-sshkey.md), [mulib-sshproto](mulib-sshproto.md), [mulib-x25519](mulib-x25519.md) |
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
