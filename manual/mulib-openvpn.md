# openvpn — data envelope and replay window

Define `MULIB_IMPL_OPENVPN`; no dependencies.
See [`examples/modules/openvpn.c`](../examples/modules/openvpn.c).

`decode`/`encode` handle OpenVPN UDP data-channel envelopes: opcode in the
upper five bits and key ID in the lower three. Supported opcodes are DATA_V1
(6, one-byte header) and DATA_V2 (9, plus a three-byte big-endian peer ID).
The remaining nonempty payload is opaque ciphertext. Decode returns 1 success,
0 malformed, -1 unsupported opcode and borrows payload bytes. Encode returns
1/0, validates key ID 0..7 and peer ID 0..0xffffff, and requires peer ID zero
for v1. `used` resets on failure; encoder buffers must not overlap.

`replay_accept` maintains a 64-packet sliding bitmap, initialized to zero per
key/session. After successful external authentication, pass the packet's uint32
ID: return 1 for a new in-window ID, 0 for duplicates, zero or packets older
than the window. Out-of-order authenticated packets are accepted once. IDs
must not wrap; rekey before exhaustion. Never call it on unauthenticated input,
which could otherwise advance the window and discard legitimate traffic.

No TLS control channel, key negotiation, packet encryption/authentication,
TCP transport framing, routing or VPN daemon is supplied. These helpers compose
with application crypto and `abstun` packet backends. Tests:
`tests/test_protocol_extensions.c` verifies window boundaries and wrap rejection.
