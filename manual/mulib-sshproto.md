# sshproto — SSH message numbers and negotiation payloads

Use KEXINIT codecs to work with the cookie, algorithm and language name lists
and `first_kex_packet_follows` flag. Parsed name lists borrow the packet, and
list validation is not negotiation. Retain the original KEXINIT payload for
the exchange hash.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-sshproto.h`](../include/mulib-sshproto.h) |
| Implementation switch | `MULIB_IMPL_SSHPROTO` |
| Repository-local includes | [mulib-sshwire](mulib-sshwire.md) |
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
