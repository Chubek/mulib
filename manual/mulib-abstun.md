# abstun — abstract packet tunnel

Define `MULIB_IMPL_ABSTUN`; no dependencies.
See [`examples/modules/abstun.c`](../examples/modules/abstun.c).

An abstract tunnel borrows a context, positive maximum packet size (MTU), and
packet-atomic receive/send callbacks. No allocation, closure or stream framing
is implicit. `send` rejects empty or oversize packets before invoking the
backend. `receive` requires output capacity at least MTU and passes exactly
MTU to the backend. This avoids handing a partial-packet-sized buffer to a
packet device. The callback must enforce its maximum packet size itself.

Callback results are 0 complete packet, 1 AGAIN, or a negative error. Successful
receives must report 1..MTU bytes; failures/AGAIN report zero. There are no EOF
packets or short successful sends. Violations return -2; invalid public
arguments return -1. Public receive counts reset on failure. Caller buffers
are borrowed during each operation. External synchronization is required.

`tuntap` supplies a native Linux backend. Other application backends can map
these operations to authenticated network packet channels. Tests:
`tests/test_transport_extensions.c` exercises MTU and callback-result checks.
