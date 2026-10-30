# bittorrent — v1 peer-wire codecs

Define `MULIB_IMPL_ENDIAN`, `MULIB_IMPL_GIT`, `MULIB_IMPL_BITTORRENT`.
See [`examples/modules/bittorrent.c`](../examples/modules/bittorrent.c).

Handshake codecs implement the 68-byte BitTorrent v1 handshake: protocol
identifier, eight reserved bytes, 20-byte info hash, 20-byte peer ID. Reserved
extension bits are preserved, not interpreted. Decode returns 1 complete,
0 incomplete, -1 invalid; it copies the result. Handshake framing is fixed-size.

Message framing is uint32 big-endian length, message ID, payload. Supported IDs
0..9 cover choke, unchoke, interested, not-interested, have, bitfield, request,
piece, cancel and port. Public ID 255 denotes a zero-length keepalive and is
never accepted as a wire ID. Frames are bounded to 1 MiB. Fixed-size messages
must match exactly; request/cancel block lengths are 1..16384, piece blocks are
1..16384, and offset-plus-size overflow is rejected. Torrent-specific piece
indices, final piece sizes and bitfield unused bits require application context.

Decode returns 1 complete, 0 incomplete, -1 malformed, -2 unsupported ID, with
used zero on failure and borrowed payload on success. Encode returns 1/0 and
requires disjoint buffers. `piece_verify` hashes a complete piece using the
shared Git SHA-1 primitive and compares a caller's expected digest.
No tracker, bencoding/metainfo parser, DHT, downloader, extension messages or
BitTorrent v2 Merkle verification is supplied. Tests:
`tests/test_protocol_extensions.c` and the standalone example.
