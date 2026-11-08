# MuLIB heap

[Manuals](README.md) · [Library index](index.md)

A binary min-heap of signed 64-bit integers. The minimum value is returned first;
the heap owns its element storage.

## Contract and behavior

Initialize with `{0}` or `init()`. This is a minimum heap of signed
64-bit values; duplicates are retained. `push()` returns 1 on success and 0 on
allocation/size failure. `pop()` removes the minimum, returning 0 when empty;
a NULL output discards it. Heap ordering is restored in O(log n), with occasional
O(n) allocation movement during growth. `free()` releases storage and zeros
state. Internal array order is heap order, not globally sorted order.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_HEAP
#include "mulib-heap.h"
int main(void) {
    mulib_heap_t h;
    mulib_heap_init(&h);
    assert(mulib_heap_push(&h, 5));
    int64_t x = 0;
    assert(mulib_heap_pop(&h, &x) && x == 5);
    mulib_heap_free(&h);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-heap.h`](../include/mulib-heap.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-heap.h
 * @brief A binary min-heap of signed 64-bit integers.
 * @defgroup mulib_heap MuLIB heap
 * @{
 * @details The minimum value is returned first. Heap adjustment takes
 * logarithmic time; growth may move all elements. Memory is owned by the heap.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_HEAP before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_HEAP_H
#define MULIB_HEAP_H
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

  /** @brief A binary min-heap of signed 64-bit integers. */
  typedef struct mulib_heap
  {
    int64_t *items;
    size_t length, capacity;
  } mulib_heap_t;
  /** @brief mulib heap init; see manual/mulib-heap.md for contracts and
   * examples. */
  void mulib_heap_init (mulib_heap_t *h);
  /** @brief mulib heap push; see manual/mulib-heap.md for contracts and
   * examples. */
  int mulib_heap_push (mulib_heap_t *h, int64_t value);
  /** @brief mulib heap pop; see manual/mulib-heap.md for contracts and
   * examples. */
  int mulib_heap_pop (mulib_heap_t *h, int64_t *out);
  /** @brief mulib heap free; see manual/mulib-heap.md for contracts and
   * examples. */
  void mulib_heap_free (mulib_heap_t *h);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_HEAP_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-heap.h`](../include/mulib-heap.h) |
| Implementation switch | `MULIB_IMPL_HEAP` |
| Repository-local includes | None |
| Complete example | [`examples/modules/heap.c`](../examples/modules/heap.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/heap.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
