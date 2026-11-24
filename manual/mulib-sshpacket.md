# sshpacket — SSH binary records

Inspect one record length and retain any incomplete input. A clear record
aligns the length plus body to eight bytes; an AES-GCM record aligns the body
to 16 bytes and adds a tag. Length inspection is not authentication: do not
interpret encrypted body bytes until the tag verifies. Packet writing requires
a random padding callback. A parsed payload borrows its plaintext body.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-sshpacket.h`](../kernels/mulib-sshpacket.h) |
| Implementation switch | `MULIB_IMPL_SSHPACKET` |
| Repository-local includes | [mulib-sshwire](mulib-sshwire.md) |
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
