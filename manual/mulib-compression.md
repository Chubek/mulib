# Explicit compression

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_COMPRESSION`, `BUFFER` and `RLE`. Explicit identity or MuLIB
count/byte RLE; this is not HTTP gzip/deflate negotiation. Decode requires a
hard output limit. Transforms atomically append and accept aliased input.
[Example](../examples/modules/compression.c), [contracts](storage-server.md).

## Transform and output contract

Initialize the destination [buffer](mulib-buffer.md) with `{0}`. Encode/decode
append to its existing bytes rather than replacing them. Both return 1 on
success and 0 for an unsupported codec, invalid input, allocation failure, or
capacity/format failure; unsuccessful calls preserve the destination. Release
it with `mulib_buffer_free`.

Identity copies input unchanged. RLE uses [count/byte pairs](mulib-rle.md),
rejecting zero counts and odd encoded lengths. Decode's `limit` bounds the
**newly decoded bytes**, not the destination's final total length. RLE validates
the expanded size before allocating temporary output. Empty input is supported;
NULL input requires length zero. The codec is selected explicitly: there is no
format detection, gzip support, or HTTP content-encoding negotiation.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-compression.h`](../include/mulib-compression.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-compression.h
 * @brief Explicit identity/RLE buffer transforms, not gzip/HTTP negotiation.
 * Define MULIB_IMPL_COMPRESSION, BUFFER and RLE. Append is failure-atomic.
 * Decoder requires a hard output limit to bound decompression expansion. */
#ifndef MULIB_COMPRESSION_H
#define MULIB_COMPRESSION_H
#include "mulib-buffer.h"
#include "mulib-rle.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum mulib_compression
  {
    MULIB_COMPRESSION_IDENTITY,
    MULIB_COMPRESSION_RLE
  } mulib_compression;
  int mulib_compression_encode (mulib_compression codec, const void *data,
                                size_t length, mulib_buffer_t *out);
  int mulib_compression_decode (mulib_compression codec, const void *data,
                                size_t length, size_t limit,
                                mulib_buffer_t *out);
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
| Header | [`include/mulib-compression.h`](../include/mulib-compression.h) |
| Implementation switch | `MULIB_IMPL_COMPRESSION` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-rle](mulib-rle.md) |
| Complete example | [`examples/modules/compression.c`](../examples/modules/compression.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/compression.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
