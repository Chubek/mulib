# MuLIB deque

[Manuals](README.md) · [Library index](index.md)

Double-ended FIFO container with a circular array. O(1) amortized insertion and removal at either end. The deque owns storage but does not own pointers stored in elements.

## Contract and behavior

Initialize fresh storage with a nonzero element stride. Push copies
one stride of bytes and returns 1, or 0 on invalid state, overflow, or allocation
failure. Existing aligned live elements may be pushed again even when growth
relocates the circular storage. Pop returns 0 when empty; a NULL output discards
the removed element. Push/pop at either end preserve logical deque order.

Growth is amortized constant-time for fixed-size elements; a growth copies all
live elements. Borrowed storage pointers can be invalidated by growth. `free()`
releases storage and zeros state. Pointees are never automatically destroyed.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_DEQUE
#include "mulib-deque.h"
int main(void) {
    mulib_deque_t d;
    mulib_deque_init(&d, sizeof(int));
    int x = 2, y = 0;
    assert(mulib_deque_push_front(&d, &x));
    assert(mulib_deque_pop_back(&d, &y) && y == 2);
    mulib_deque_free(&d);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-deque.h`](../include/mulib-deque.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-deque.h
 * @brief Double-ended FIFO container with a circular array.
 * @defgroup mulib_deque MuLIB deque
 * @{
 * @details O(1) amortized insertion and removal at either end. The deque owns
 * storage but does not own pointers stored in elements.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_DEQUE before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_DEQUE_H
#define MULIB_DEQUE_H
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

  /** @brief Double-ended FIFO container with a circular array. */
  typedef struct mulib_deque
  {
    unsigned char *data;
    size_t head, length, capacity, stride;
  } mulib_deque_t;
  /** @brief mulib deque init; see manual/mulib-deque.md for contracts and
   * examples. */
  void mulib_deque_init (mulib_deque_t *d, size_t stride);
  /** @brief Copy to the front; sources may be existing aligned live slots,
   * even across growth. */
  int mulib_deque_push_front (mulib_deque_t *d, const void *x);
  /** @brief Copy to the back; sources may be existing aligned live slots, even
   * across growth. */
  int mulib_deque_push_back (mulib_deque_t *d, const void *x);
  /** @brief mulib deque pop front; see manual/mulib-deque.md for contracts and
   * examples. */
  int mulib_deque_pop_front (mulib_deque_t *d, void *out);
  /** @brief mulib deque pop back; see manual/mulib-deque.md for contracts and
   * examples. */
  int mulib_deque_pop_back (mulib_deque_t *d, void *out);
  /** @brief mulib deque free; see manual/mulib-deque.md for contracts and
   * examples. */
  void mulib_deque_free (mulib_deque_t *d);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_DEQUE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-deque.h`](../include/mulib-deque.h) |
| Implementation switch | `MULIB_IMPL_DEQUE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/deque.c`](../examples/modules/deque.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/deque.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
