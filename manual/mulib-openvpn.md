# openvpn — data envelope and replay window

[Manuals](README.md) · [Library index](index.md)

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

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-openvpn.h`](../include/mulib-openvpn.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-openvpn.h
 * @brief OpenVPN data-channel v1/v2 envelopes and authenticated replay window.
 * Define MULIB_IMPL_OPENVPN once. Encryption, TLS control channel, key
 * negotiation and packet authentication are supplied by the application.
 */
#ifndef MULIB_OPENVPN_H
#define MULIB_OPENVPN_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_openvpn_packet
  {
    unsigned opcode, key_id;
    uint32_t peer_id;
    const unsigned char *payload;
    size_t length;
  } mulib_openvpn_packet_t;
  typedef struct mulib_openvpn_replay
  {
    uint64_t seen;
    uint32_t highest;
  } mulib_openvpn_replay_t;
  /** @brief Decode UDP envelope opcodes 6 (DATA_V1) or 9 (DATA_V2). 1 success,
   * 0 malformed, -1 unsupported opcode. Borrow payload without authentication.
   */
  int mulib_openvpn_decode (const void *, size_t, mulib_openvpn_packet_t *);
  /** @brief Encode data envelope, key_id 0..7, peer_id 24 bits for v2; v1
   * requires peer_id=0. 1 success/0 failure; used reset on failure. */
  int mulib_openvpn_encode (const mulib_openvpn_packet_t *, void *, size_t,
                            size_t *used);
  /** @brief Accept authenticated packet id in a 64-packet sliding window.
   * Zero-initialize per key/session. 1 new, 0 duplicate/too old/zero. Call
   * ONLY after MAC/AEAD verification; IDs cannot wrap (rekey first). */
  int mulib_openvpn_replay_accept (mulib_openvpn_replay_t *, uint32_t id);
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
| Header | [`include/mulib-openvpn.h`](../include/mulib-openvpn.h) |
| Implementation switch | `MULIB_IMPL_OPENVPN` |
| Repository-local includes | None |
| Complete example | [`examples/modules/openvpn.c`](../examples/modules/openvpn.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/openvpn.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
