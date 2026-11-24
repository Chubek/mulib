# sshwire — SSH wire cursors

Use caller-owned reader and writer cursors for big-endian integers, SSH
strings and canonical nonnegative mpints. Reads advance only on success and
returned byte spans borrow the input. Encoding an mpint adds a sign octet when
the high bit would otherwise make a nonnegative value look negative.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-sshwire.h`](../kernels/mulib-sshwire.h) |
| Implementation switch | `MULIB_IMPL_SSHWIRE` |
| Repository-local includes | [mulib-endian](mulib-endian.md) |
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
