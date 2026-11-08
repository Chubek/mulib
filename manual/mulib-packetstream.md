# packetstream — incremental length framing

[Manuals](README.md) · [Library index](index.md)

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

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-packetstream.h`](../include/mulib-packetstream.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-packetstream.h
 * @brief Incremental bounded four-byte big-endian length framing.
 * Define MULIB_IMPL_PACKETSTREAM and MULIB_IMPL_ENDIAN once.
 */
#ifndef MULIB_PACKETSTREAM_H
#define MULIB_PACKETSTREAM_H
#include "mulib-endian.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_packetstream
  {
    unsigned char header[4], *buffer;
    size_t header_used, used, length, capacity;
    int failed;
  } mulib_packetstream_t;
  /** @brief Initialize a decoder borrowing caller storage, capacity <=
   * UINT32_MAX. */
  int mulib_packetstream_init (mulib_packetstream_t *, void *storage,
                               size_t capacity);
  /** @brief Consume arbitrary fragments. Callback borrows each complete
   * payload (including empty packets). Returns 0 success, 1 callback stop, -1
   * invalid, -2 oversized packet (sticky). *consumed always reports accepted
   * input. Callback must not reenter or mutate the decoder. */
  int mulib_packetstream_feed (mulib_packetstream_t *, const void *, size_t,
                               size_t *consumed,
                               int (*packet) (void *, const void *, size_t),
                               void *);
  /** @brief Encode one packet; nonoverlapping output, 1 success, 0 failure. */
  int mulib_packetstream_encode (const void *, size_t, void *, size_t,
                                 size_t *);
  /** @brief Return 1 only at a clean frame boundary, suitable for checking
   * EOF. */
  int mulib_packetstream_finished (const mulib_packetstream_t *);
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
| Header | [`include/mulib-packetstream.h`](../include/mulib-packetstream.h) |
| Implementation switch | `MULIB_IMPL_PACKETSTREAM` |
| Repository-local includes | [mulib-endian](mulib-endian.md) |
| Complete example | [`examples/modules/packetstream.c`](../examples/modules/packetstream.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/packetstream.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
