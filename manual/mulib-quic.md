# QUIC wire helpers

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_QUIC` and `BUFFER`. RFC 9000 variable integers up to 2^62-1,
plus length-bearing STREAM frames with optional offset/FIN. Read returns 1 OK,
0 truncated, -1 invalid. Payload views borrow input; writes append atomically.
No packet protection, TLS, ACKs, congestion or network transport. Do not send
these frames as unauthenticated packets. [Example](../examples/modules/quic.c),
[limits](storage-server.md#http-and-policies).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-quic.h`](../include/mulib-quic.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-quic.h
 * @brief RFC 9000 variable integers and length-bearing STREAM frame codecs.
 * Define MULIB_IMPL_QUIC and BUFFER. No packet protection, TLS, congestion,
 * ACK processing or transport; never send these as unauthenticated packets.
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
  typedef struct mulib_quic_stream
  {
    uint64_t stream_id, offset;
    const unsigned char *data;
    size_t length;
    int fin;
  } mulib_quic_stream_t;
  int mulib_quic_stream_read (const void *data, size_t length,
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
