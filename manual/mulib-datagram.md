# datagram — UDP endpoints

Define `MULIB_IMPL_FILECTL` and `MULIB_IMPL_DATAGRAM`.
See [`examples/modules/datagram.c`](../examples/modules/datagram.c).

POSIX sockets support IPv4 and IPv6. Addresses contain network-order address
bytes, a host-order port, family 4/6, and IPv6 scope ID. No DNS or text parsing
occurs. Initialize owning descriptors with `MULIB_DATAGRAM_INIT`; `open` binds
one, sets close-on-exec and optional nonblocking mode, and reports the actual
local address (port zero selects an ephemeral port). Failure leaves outputs
unchanged and closes any intermediate descriptor. Do not copy a live owner.

`send` transmits an entire packet, including an empty packet, with a portable
65507-byte maximum. `receive` reports the sender and copied length. A truncated
packet is consumed and returns TRUNCATED (2); the copied prefix length is valid.
An empty packet returns OK, not EOF. A zero-capacity receive still consumes a
packet and reports truncation if needed. Interrupted send/receive calls retry.

Results: 0 OK, 1 AGAIN, 2 TRUNCATED (receive only), -1 invalid, -2 OS error,
-3 unsupported platform. Counts reset on errors; OS failures preserve errno.
`close` resets ownership and never retries close after EINTR. No connection,
multicast, authentication or fragmentation-reassembly API is supplied.
Tests: `tests/test_ipc_extensions.c`, using unprivileged loopback UDP.
