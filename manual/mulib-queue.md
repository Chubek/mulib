# MuLIB queue

[Manuals](README.md) · [Library index](index.md)

A dynamically growing FIFO queue implemented with a circular buffer. The queue preserves insertion order even after wraparound and relocation. Push may invalidate pointers into its storage.

## Contract and behavior

Initialize fresh storage with a nonzero byte stride. Push copies one
element and returns 1, or 0 on invalid state, size overflow, or allocation
failure. An aligned live element may be reused as input even across growth.
Pop removes the oldest element, returns 0 when empty, and allows NULL output
to discard an element. Growth can move/relinearize the circular storage and
invalidate borrowed pointers. Operations are amortized constant-time for fixed
stride. `free()` releases storage and zeros state; pointed-to objects remain
caller-owned.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_QUEUE
#include "mulib-queue.h"
int main(void) {
    mulib_queue_t q;
    mulib_queue_init(&q, sizeof(int));
    int x = 4, y = 0;
    assert(mulib_queue_push(&q, &x));
    assert(mulib_queue_pop(&q, &y) && y == 4);
    mulib_queue_free(&q);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-queue.h`](../include/mulib-queue.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-queue.h
 * @brief A dynamically growing FIFO queue implemented with a circular buffer.
 * @defgroup mulib_queue MuLIB queue
 * @{
 * @details The queue preserves insertion order even after wraparound and
 * relocation. Push may invalidate pointers into its storage.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_QUEUE before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_QUEUE_H
#define MULIB_QUEUE_H
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

  /** @brief A dynamically growing FIFO queue implemented with a circular
   * buffer. */
  typedef struct mulib_queue
  {
    unsigned char *data;
    size_t head, length, capacity, stride;
  } mulib_queue_t;
  /** @brief mulib queue init; see manual/mulib-queue.md for contracts and
   * examples. */
  void mulib_queue_init (mulib_queue_t *q, size_t stride);
  /** @brief Copy an element; existing aligned live slots may be sources across
   * growth. */
  int mulib_queue_push (mulib_queue_t *q, const void *item);
  /** @brief mulib queue pop; see manual/mulib-queue.md for contracts and
   * examples. */
  int mulib_queue_pop (mulib_queue_t *q, void *out);
  /** @brief mulib queue free; see manual/mulib-queue.md for contracts and
   * examples. */
  void mulib_queue_free (mulib_queue_t *q);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_QUEUE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-queue.h`](../include/mulib-queue.h) |
| Implementation switch | `MULIB_IMPL_QUEUE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/queue.c`](../examples/modules/queue.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/queue.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
