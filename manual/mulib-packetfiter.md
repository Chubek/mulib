# packetfiter — IP metadata parsing

Define `MULIB_IMPL_PACKETFITER`; no dependencies. The requested filename and
symbol spelling are retained. See
[`examples/modules/packetfiter.c`](../examples/modules/packetfiter.c).

`parse` borrows a raw IP packet (no Ethernet header) and copies metadata into a
value object. IPv4 validates version, IHL, total length and reserved fragment
bit, skips options, and extracts addresses/protocol. Fragments with DF set,
empty payloads, nonfinal payload sizes not divisible by eight, or offsets and
lengths exceeding the maximum reassembled datagram size are malformed. Any
fragmented IPv4 packet has `fragmented=1` and never exposes ports.
IPv6 accepts the fixed header and plain next-header payloads; extension headers
0/43/44/50/51/60/135/139/140/253/254 explicitly
return unsupported. IPv6 jumbograms and reassembly are outside this profile.

Unfragmented TCP requires its minimum header and valid data offset; UDP requires
a minimum header and UDP length matching IP payload length. Ports are host-order
integers. Address arrays are network-order bytes, with unused IPv4 bytes zero.
`length` excludes trailing link padding and `payload_offset` locates transport
payload when understood, otherwise the IP payload. Unknown non-extension
protocols retain their protocol number and do not expose ports.

Return 1 success, 0 truncated/malformed, -1 invalid argument, -2 unsupported.
Output remains unchanged on failure. There is no checksum verification,
allocation or packet mutation. Tests: `tests/test_protocol_extensions.c`.
