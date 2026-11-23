# QUIC wire helpers

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_QUIC` and `MULIB_IMPL_BUFFER`. The codecs accept RFC 9000
variable integers up to 2^62-1 and plaintext STREAM and ACK/ACK_ECN frames.
Read returns 1 OK, 0 truncated, or -1 invalid. STREAM views borrow input.
Writes append atomically to an initialized buffer.

`mulib_quic_stream_read` requires an explicit length, as before.
`mulib_quic_packet_stream_read` also accepts STREAM frames whose length
is implicit in the remaining packet payload. Give it only the bytes left
in the authenticated plaintext packet.

ACK decoding writes inclusive, descending ranges into caller-provided storage.
The capacity includes the first range. A declared range count above capacity
returns -2 before parsing ranges. ECN counters and the encoded delay are
returned unchanged in wire units. All outputs stay unchanged on failure.
The encoder rejects reversed, overlapping, and adjacent ranges.

Packet protection, TLS, loss recovery, and UDP transport are outside the
codec. [Example](../examples/modules/quic.c),
[limits](storage-server.md#http-and-policies).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-quic.h`](../include/mulib-quic.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-quic.h
 * @brief RFC 9000 variable integers, STREAM and ACK frame codecs.
 * Define MULIB_IMPL_QUIC and BUFFER. These codecs operate on plaintext frame
 * payloads. They provide no packet protection, TLS, recovery or transport.
 * Parse: 1 OK, 0 truncated, -1 invalid. Views borrow input; failure unchanged.
 */
#ifndef MULIB_QUIC_H
#define MULIB_QUIC_H
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C"
{
#endif
#define MULIB_QUIC_MAX_INTEGER UINT64_C (0x3fffffffffffffff)
  int mulib_quic_varint_read (const void *data, size_t length, uint64_t *value,
                              size_t *consumed);
  int mulib_quic_varint_write (mulib_buffer_t *out, uint64_t value);
  typedef struct mulib_quic_ack_range
  {
    uint64_t smallest, largest;
  } mulib_quic_ack_range_t;
  typedef struct mulib_quic_ack
  {
    uint64_t delay, ect0, ect1, ce;
    size_t range_count;
    int ecn;
  } mulib_quic_ack_t;
  /** Parse ACK/ACK_ECN into descending inclusive ranges. Returns -2 when
   * range_capacity is too small. All outputs are unchanged on failure. */
  int mulib_quic_ack_read (const void *data, size_t length,
                           mulib_quic_ack_t *out, mulib_quic_ack_range_t *ranges,
                           size_t range_capacity, size_t *consumed);
  /** Append ACK/ACK_ECN with descending, disjoint and nonadjacent ranges. */
  int mulib_quic_ack_write (mulib_buffer_t *out, const mulib_quic_ack_t *ack,
                            const mulib_quic_ack_range_t *ranges);
  typedef struct mulib_quic_stream
  {
    uint64_t stream_id, offset;
    const unsigned char *data;
    size_t length;
    int fin;
  } mulib_quic_stream_t;
  int mulib_quic_stream_read (const void *data, size_t length,
                              mulib_quic_stream_t *out, size_t *consumed);
  /** Like stream_read, but also accepts implicit-length STREAM frames.
   * Supply only the remaining plaintext packet payload as input. */
  int mulib_quic_packet_stream_read (const void *data, size_t length,
                                     mulib_quic_stream_t *out, size_t *consumed);
  int mulib_quic_stream_write (mulib_buffer_t *out,
                               const mulib_quic_stream_t *stream);
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
| Header | [`include/mulib-quic.h`](../include/mulib-quic.h) |
| Implementation switch | `MULIB_IMPL_QUIC` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md) |
| Complete example | [`examples/modules/quic.c`](../examples/modules/quic.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/quic.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
