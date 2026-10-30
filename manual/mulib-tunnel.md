# tunnel — duplex stream relay

Define `MULIB_IMPL_XTKS` and `MULIB_IMPL_TUNNEL`.
See [`examples/modules/tunnel.c`](../examples/modules/tunnel.c).

Initialize with two full-duplex XTK streams and two disjoint, nonempty buffers
of the same capacity. Streams, contexts and buffers are borrowed for the
relay's lifetime. There is no descriptor closure or allocation. Each direction
tracks pending bytes independently. A source is read only after its prior
buffer has drained, so destination short writes and AGAIN do not lose data.

`step` performs at most one read and one write per direction. Return 0 for
progress, 1 when both directions would block without progress, 2 once both
inputs have reached EOF and all pending output has drained, or a negative
sticky failure. Call again after readiness notification; the relay does not
busy-wait or poll sockets. A blocking backend may block within a step.

One source's EOF does not close the opposite stream or discard its pending
data. Applications implement protocol-specific half-close and timeout policy.
Initialization resets a relay but cannot close a previously supplied backend.
This module relays bytes; packet tunnels use `abstun` instead.
Tests: `tests/test_transport_extensions.c`, with independently blocked lanes.
