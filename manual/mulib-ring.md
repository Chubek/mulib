# MuLIB ring

[Manuals](README.md) · [Library index](index.md)

Fixed-capacity circular buffer which refuses to overwrite unread items. Capacity is fixed at initialization, and writes fail without mutation when full.

## Contract and behavior

Init requires positive capacity and byte stride, allocates fixed
storage, and returns 1 on success or 0 on failure. Write copies one element,
returning 0 when full instead of overwriting unread data. Read removes the
oldest element, returning 0 when empty; NULL output discards it. Both are
constant-time for fixed stride. `free()` releases storage and zeros state.
Free before reinitializing; stored pointers do not transfer pointee ownership.
This ring supplies no atomic producer/consumer synchronization.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_RING
#include "mulib-ring.h"
int main(void) {
    mulib_ring_t r;
    assert(mulib_ring_init(&r, 4, sizeof(int)));
    int x = 9, y = 0;
    assert(mulib_ring_write(&r, &x));
    assert(mulib_ring_read(&r, &y) && y == 9);
    mulib_ring_free(&r);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ring.h`](../include/mulib-ring.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-ring.h
 * @brief Fixed-capacity circular buffer which refuses to overwrite unread
 * items.
 * @defgroup mulib_ring MuLIB ring
 * @{
 * @details Capacity is fixed at initialization, and writes fail without
 * mutation when full.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_RING before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_RING_H
#define MULIB_RING_H
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

  /** @brief Fixed-capacity circular buffer which refuses to overwrite unread
   * items. */
  typedef struct mulib_ring
  {
    unsigned char *data;
    size_t head, length, capacity, stride;
  } mulib_ring_t;
  /** @brief mulib ring init; see manual/mulib-ring.md for contracts and
   * examples. */
  int mulib_ring_init (mulib_ring_t *r, size_t capacity, size_t stride);
  /** @brief mulib ring write; see manual/mulib-ring.md for contracts and
   * examples. */
  int mulib_ring_write (mulib_ring_t *r, const void *x);
  /** @brief mulib ring read; see manual/mulib-ring.md for contracts and
   * examples. */
  int mulib_ring_read (mulib_ring_t *r, void *out);
  /** @brief mulib ring free; see manual/mulib-ring.md for contracts and
   * examples. */
  void mulib_ring_free (mulib_ring_t *r);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_RING_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ring.h`](../include/mulib-ring.h) |
| Implementation switch | `MULIB_IMPL_RING` |
| Repository-local includes | None |
| Complete example | [`examples/modules/ring.c`](../examples/modules/ring.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ring.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
