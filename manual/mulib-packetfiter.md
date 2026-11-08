# packetfiter — IP metadata parsing

[Manuals](README.md) · [Library index](index.md)

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

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-packetfiter.h`](../include/mulib-packetfiter.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-packetfiter.h
 * @brief Bounds-checked IP packet metadata for filtering (historical
 * spelling). Define MULIB_IMPL_PACKETFITER once. No checksum verification or
 * reassembly.
 */
#ifndef MULIB_PACKETFITER_H
#define MULIB_PACKETFITER_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_packetfiter_view
  {
    unsigned family, protocol;
    unsigned char source[16], destination[16];
    uint16_t source_port, destination_port;
    int fragmented, has_ports;
    size_t length, payload_offset;
  } mulib_packetfiter_view_t;
  /** @brief Parse an IP packet, permitting trailing link padding. IPv4 options
   * are skipped; IPv6 extension headers are explicitly unsupported (-2).
   * 1 success, 0 truncated/malformed, -1 invalid arguments, -2 unsupported.
   * Fragmented IPv4 never exposes ports; contradictory DF, empty fragments,
   * misaligned nonfinal fragments and impossible reassembly sizes are rejected.
   * Output unchanged on failure. */
  int mulib_packetfiter_parse (const void *, size_t,
                               mulib_packetfiter_view_t *);
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
| Header | [`include/mulib-packetfiter.h`](../include/mulib-packetfiter.h) |
| Implementation switch | `MULIB_IMPL_PACKETFITER` |
| Repository-local includes | None |
| Complete example | [`examples/modules/packetfiter.c`](../examples/modules/packetfiter.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/packetfiter.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
