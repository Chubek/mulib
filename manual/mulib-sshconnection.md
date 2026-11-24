# sshconnection — Socket-independent SSH connection driver

The driver owns protocol sequencing from identification through authentication
and session channels. Supply a trusted host pin for a client, or a host seed
and an attempt-limited account policy for a server. The application supplies
the channel slot storage and makes channel authorization decisions.

The [in-memory example](../examples/ssh/connection.c) does a signed login and
exchanges channel data with intentionally short writes.

When writable, send the byte span from `mulib_sshconnection_output` and
consume only the number of bytes the socket accepted. Pass received bytes to
`mulib_sshconnection_feed`, honoring `used` and retaining any unaccepted
suffix. Call it again with no input to drain buffered bytes. Handle borrowed
events before the next feed. Drive both readable and writable events even
while a write is incomplete, and call `mulib_sshconnection_clear` at teardown.
The driver responds to peer-initiated rekey; it does not initiate rekey.
Use direct transport APIs when the application must initiate key rotation.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-sshconnection.h`](../kernels/mulib-sshconnection.h) |
| Implementation switch | `MULIB_IMPL_SSHCONNECTION` |
| Repository-local includes | [mulib-sshchannel](mulib-sshchannel.md), [mulib-sshserver](mulib-sshserver.md) |
| Complete example | [`examples/ssh/connection.c`](../examples/ssh/connection.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/ssh/connection.c -pthread -lm -ldl -lcrypto -o mulib-example
```
<!-- /manual-reference -->
