# sshchannel — SSH session channels

Use after authentication. Open and confirm a `session` channel with separate
local and remote ID namespaces. Standard and extended data share the same
flow-control window. Grant window credit only after the application has consumed
buffered data. Only reuse a channel ID after both peers have sent `CLOSE`;
`EOF` closes only one direction of data flow.

The application chooses channel IDs and dispatches requests. Request names and
bodies are not authorization: validate each supported operation before acting.
Receive events borrow the input; process them before reusing the buffer.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-sshchannel.h`](../include/mulib-sshchannel.h) |
| Implementation switch | `MULIB_IMPL_SSHCHANNEL` |
| Repository-local includes | [mulib-sshpacket](mulib-sshpacket.md), [mulib-sshproto](mulib-sshproto.md) |
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
