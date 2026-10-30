# packetstream — incremental length framing

Define `MULIB_IMPL_ENDIAN` and `MULIB_IMPL_PACKETSTREAM`.
See [`examples/modules/packetstream.c`](../examples/modules/packetstream.c).

Wire format is an unsigned 32-bit big-endian payload length followed by exactly
that many bytes. Zero-length packets are valid. `init` borrows caller storage
and a maximum payload capacity (0..UINT32_MAX). No allocation occurs. `feed`
accepts arbitrary fragments, including several frames per call, and copies
payload bytes into the borrowed storage. Header storage is internal.

The callback sees each complete packet and borrows it only during that call.
Return nonzero to stop after that packet; the decoder is already reset to a
frame boundary. `consumed` identifies the input suffix to retain. Do not
reenter or mutate the decoder from the callback. Return codes: 0 input accepted,
1 callback stop, -1 invalid argument, -2 declared length exceeds capacity.
Oversize failure is sticky until reinitialization; no implicit resynchronization
is attempted. `finished` verifies a clean EOF boundary, detecting truncated
headers and payloads. Empty `feed` does not manufacture a packet.

`encode` writes one complete frame to nonoverlapping caller output and returns
1/0. `used` is zero on failure. This framing supplies neither authentication nor
message semantics. Tests: `tests/test_transport_extensions.c`.
