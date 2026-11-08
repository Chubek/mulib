# mulib-mumq-server

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_MUMQ_SERVER` plus `MULIB_IMPL_MUMQ`, `MULIB_IMPL_SOCKET`,
`MULIB_IMPL_AES`, `MULIB_IMPL_AUTHCRYPTO`. The broker owns up to 32 channels;
`server_add` transfers ownership only on zero, `remove`/destroy close them.
Slot numbers are local handles and are reused after removal, not authenticated
remote identities. Associate provisioned peer identities with slots in your
authorization context. `examples/modules/mumq-server.c` is runnable.

Each connection has one replaceable exact-topic subscription. The server
enqueues SUBACK before subsequently routing to the new subscription. Publishers
can send any permitted topic; subscribers receive every matching publication,
including their own if applicable. No wildcard, retained message, offline queue,
disk persistence, subscription list or automatic reconnection is claimed.

`step` is a fair bounded nonblocking pass. It flushes each output then receives
at most one message per publisher. If a subscriber's queue is full, the publisher
retains one pending message and a recipient cursor: already queued copies are
not duplicated on retry. Other peers can progress. This bounds broker memory
but allows a slow subscriber to stall a publisher indefinitely; the application
can disconnect stalled slots. Membership is evaluated as the cursor advances,
not a transactional snapshot. Delivery after disconnect/restart is not assured.

An optional borrowed authorization callback permits/rejects publish/subscribe
by slot/topic. Rejection, malformed traffic, SUBACK from a client, EOF, and
transport failures disconnect the offending peer; `step` returns the first
error while continuing other peers. NULL authorization permits all peers:
only use that in a trusted mesh. Shared keys alone do not distinguish peers or
prevent a permitted publisher from forging an application result. Provision
per-peer credentials and ACLs for an untrusted network.

The application owns numeric TCP listeners and accepts channels using internal
`mulib-socket` APIs. It selects credentials/options for each accepted connection
and calls add. There are no hidden threads, background daemons or DNS. See
`mulib-mumq.md` for framing, encryption and implementation-placement contracts.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-mumq-server.h`](../include/mulib-mumq-server.h) |
| Implementation switch | `MULIB_IMPL_MUMQ_SERVER` |
| Repository-local includes | [mulib-mumq](mulib-mumq.md) |
| Complete example | [`examples/modules/mumq-server.c`](../examples/modules/mumq-server.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/mumq-server.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
