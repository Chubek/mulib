# socks5 — RFC 1928 codecs

[Manuals](README.md) · [Library index](index.md)

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

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-socks5.h`](../include/mulib-socks5.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-socks5.h
 * @brief RFC 1928 method negotiation, address and CONNECT/BIND/UDP codecs.
 * Define MULIB_IMPL_SOCKS5 once. No socket ownership or authentication engine.
 */
#ifndef MULIB_SOCKS5_H
#define MULIB_SOCKS5_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_socks5_address
  {
    unsigned type; /* 1 IPv4, 3 domain, 4 IPv6 */
    unsigned char host[255];
    size_t length;
    uint16_t port;
  } mulib_socks5_address_t;
  /** @brief Decode/encode address fields. Decode returns 1 success, 0 need
   * more, -1 malformed. Encode returns 1/0. used is zero on failure.
   * Domain is 1..255 bytes, no NUL/control bytes, no DNS interpretation. */
  int mulib_socks5_address_decode (const void *, size_t,
                                   mulib_socks5_address_t *, size_t *used);
  int mulib_socks5_address_encode (const mulib_socks5_address_t *, void *,
                                   size_t, size_t *used);
  /** @brief Parse client greeting, select first server-preferred offered
   * method. Output is [5,method] or [5,255] for no match; 1/0/-1 as decode
   * above. */
  int mulib_socks5_negotiate (const void *, size_t,
                              const unsigned char *preferred, size_t count,
                              unsigned char reply[2], size_t *used);
  /** @brief Decode request/reply header. reply=0 validates command 1..3;
   * reply=1 validates reply code 0..8. Reserved byte must be zero. */
  int mulib_socks5_decode (const void *, size_t, int reply, unsigned *code,
                           mulib_socks5_address_t *, size_t *used);
  /** @brief Encode a request/reply header. Nonoverlapping source/output. */
  int mulib_socks5_encode (unsigned code, int reply,
                           const mulib_socks5_address_t *, void *, size_t,
                           size_t *used);
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
| Header | [`include/mulib-socks5.h`](../include/mulib-socks5.h) |
| Implementation switch | `MULIB_IMPL_SOCKS5` |
| Repository-local includes | None |
| Complete example | [`examples/modules/socks5.c`](../examples/modules/socks5.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/socks5.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
