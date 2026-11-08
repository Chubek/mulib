# MuLIB rle

[Manuals](README.md) · [Library index](index.md)

Byte run-length encoding and bounded decoding. Wire format repeats pairs `(count, byte)` where counts range 1..255. No checksum or length header is included; compressed data can be larger than source.

## Contract and behavior

The wire format is `(count, byte)` pairs, with counts 1..255. Longer
runs split into multiple pairs; worst-case encoded size is twice the input
length. Encode returns the byte count, or zero on insufficient capacity/invalid
input; empty success also returns zero. Encode may leave partial output on
capacity failure.

Decode rejects odd input lengths, zero counts, and expansion beyond output
capacity. It validates the complete stream before writing, returns 1 on success
or 0 on failure, and updates optional `written` only on success. Data is binary
with no terminator; no allocation occurs. Use disjoint input/output ranges.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_RLE
#include "mulib-rle.h"
int main(void) {
    unsigned char out[32];
    unsigned char in[] = {1, 1, 2, 2};
    assert(mulib_rle_encode(in, 4, out, sizeof out) == 4);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-rle.h`](../include/mulib-rle.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-rle.h
 * @brief Byte run-length encoding and bounded decoding.
 * @defgroup mulib_rle MuLIB rle
 * @{
 * @details Wire format repeats pairs `(count, byte)` where counts
 * range 1..255. No checksum or length header is included; compressed data can
 * be larger than source.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_RLE before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_RLE_H
#define MULIB_RLE_H
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief mulib rle encode; see manual/mulib-rle.md for contracts and
   * examples. */
  size_t mulib_rle_encode (const unsigned char *in, size_t n,
                           unsigned char *out, size_t cap);
  /** @brief Decode disjoint buffers, leaving output/written unchanged on
   * malformed data or insufficient capacity. */
  int mulib_rle_decode (const unsigned char *in, size_t n, unsigned char *out,
                        size_t cap, size_t *written);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_RLE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-rle.h`](../include/mulib-rle.h) |
| Implementation switch | `MULIB_IMPL_RLE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/rle.c`](../examples/modules/rle.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/rle.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
