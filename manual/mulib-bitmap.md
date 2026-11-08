# MuLIB bitmap

[Manuals](README.md) · [Library index](index.md)

Packed one-bit raster image with per-pixel access. Useful for monochrome masks and occupancy maps. Coordinates are zero-based; invalid reads return zero and invalid writes fail.

## Contract and behavior

Initialize fresh or zeroed storage with width and height. Pixels start
clear and use packed row-major bits (`y * width + x`, low bit first within each
byte). Zero dimensions are accepted. `init()` and `put()` return 1 on success,
0 on invalid coordinates/dimensions or allocation failure. `get()` returns 0
for both a clear pixel and invalid coordinates. `free()` releases pixels and
zeros the structure. Free before reinitializing; the bitmap owns no other data.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_BITMAP
#include "mulib-bitmap.h"
int main(void) {
    mulib_bitmap_t b;
    assert(mulib_bitmap_init(&b, 10, 10));
    assert(mulib_bitmap_put(&b, 3, 4, 1));
    assert(mulib_bitmap_get(&b, 3, 4));
    mulib_bitmap_free(&b);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-bitmap.h`](../include/mulib-bitmap.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-bitmap.h
 * @brief Packed one-bit raster image with per-pixel access.
 * @defgroup mulib_bitmap MuLIB bitmap
 * @{
 * @details Useful for monochrome masks and occupancy maps. Coordinates are
 * zero-based; invalid reads return zero and invalid writes fail.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_BITMAP before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_BITMAP_H
#define MULIB_BITMAP_H
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

  /** @brief Packed one-bit raster image with per-pixel access. */
  typedef struct mulib_bitmap
  {
    unsigned char *pixels;
    size_t width, height;
  } mulib_bitmap_t;
  /** @brief mulib bitmap init; see manual/mulib-bitmap.md for contracts and
   * examples. */
  int mulib_bitmap_init (mulib_bitmap_t *b, size_t width, size_t height);
  /** @brief mulib bitmap get; see manual/mulib-bitmap.md for contracts and
   * examples. */
  int mulib_bitmap_get (const mulib_bitmap_t *b, size_t x, size_t y);
  /** @brief mulib bitmap put; see manual/mulib-bitmap.md for contracts and
   * examples. */
  int mulib_bitmap_put (mulib_bitmap_t *b, size_t x, size_t y, int on);
  /** @brief mulib bitmap free; see manual/mulib-bitmap.md for contracts and
   * examples. */
  void mulib_bitmap_free (mulib_bitmap_t *b);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_BITMAP_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-bitmap.h`](../include/mulib-bitmap.h) |
| Implementation switch | `MULIB_IMPL_BITMAP` |
| Repository-local includes | None |
| Complete example | [`examples/modules/bitmap.c`](../examples/modules/bitmap.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bitmap.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
