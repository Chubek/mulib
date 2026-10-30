# socks5 — RFC 1928 codecs

Define `MULIB_IMPL_SOCKS5`; no dependencies.
See [`examples/modules/socks5.c`](../examples/modules/socks5.c).

Address codecs support ATYP 1 IPv4, 3 domain, 4 IPv6, followed by a big-endian
port. The public port is host-order. Domain bytes are 1..255 bytes and exclude
NUL, controls and spaces; they need not be NUL-terminated. IPv4/IPv6 lengths
must be exactly 4/16. No DNS interpretation or address allocation occurs.

`negotiate` consumes a complete version-5 method greeting and selects the first
server-preferred method offered by the client. No match returns `[5,255]` as
a successful protocol response; authentication itself is not performed.
`decode`/`encode` handle version, command/reply, reserved byte and address:
request commands CONNECT/BIND/UDP ASSOCIATE (1..3), reply codes 0..8. The reserved
byte must be zero. Choosing a command only encodes it; it does not open sockets.

Decoders return 1 complete, 0 incomplete, -1 malformed/invalid, set `used=0`
on failure and commit outputs only on success. Retain incomplete bytes for a
later call. Encoders return 1/0 and require disjoint output/input. No state is
retained. RFC 1929 authentication, SOCKS UDP relay envelopes and connection
management are outside these APIs. Tests: `tests/test_protocol_extensions.c`.
