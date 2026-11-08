# bittorrent — v1 peer-wire codecs

[Manuals](README.md) · [Library index](index.md)

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

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-bittorrent.h`](../include/mulib-bittorrent.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-bittorrent.h
 * @brief BitTorrent v1 peer handshake, bounded peer-wire messages and piece
 * SHA-1. Define MULIB_IMPL_BITTORRENT, MULIB_IMPL_ENDIAN and MULIB_IMPL_GIT
 * once.
 */
#ifndef MULIB_BITTORRENT_H
#define MULIB_BITTORRENT_H
#include "mulib-endian.h"
#include "mulib-git.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_bittorrent_handshake
  {
    unsigned char reserved[8], info_hash[20], peer_id[20];
  } mulib_bittorrent_handshake_t;
  typedef struct mulib_bittorrent_message
  {
    unsigned id;
    const unsigned char *data;
    size_t length;
  } mulib_bittorrent_message_t;
  /** @brief Handshake codec, 68 bytes. Decode 1 complete, 0 incomplete, -1
   * invalid; encode 1/0. Reserved extension bits preserved, not interpreted.
   */
  int mulib_bittorrent_handshake_encode (const mulib_bittorrent_handshake_t *,
                                         void *, size_t);
  int mulib_bittorrent_handshake_decode (const void *, size_t,
                                         mulib_bittorrent_handshake_t *);
  /** @brief Peer-wire message ids 0..9, 255 keepalive. Data excludes
   * id/length. Maximum frame 1 MiB, piece blocks <=16 KiB, requests nonzero
   * <=16 KiB. 1 complete, 0 incomplete, -1 malformed, -2 unsupported id.
   * used=0 on error. */
  int mulib_bittorrent_decode (const void *, size_t,
                               mulib_bittorrent_message_t *, size_t *used);
  /** @brief Encode validated message, 1 success/0 invalid/capacity. */
  int mulib_bittorrent_encode (const mulib_bittorrent_message_t *, void *,
                               size_t, size_t *used);
  /** @brief Compare SHA-1 of a complete piece with expected 20-byte digest. */
  int mulib_bittorrent_piece_verify (const void *, size_t,
                                     const unsigned char expected[20]);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-bittorrent.h`](../include/mulib-bittorrent.h) |
| Implementation switch | `MULIB_IMPL_BITTORRENT` |
| Repository-local includes | [mulib-endian](mulib-endian.md), [mulib-git](mulib-git.md) |
| Complete example | [`examples/modules/bittorrent.c`](../examples/modules/bittorrent.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bittorrent.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
