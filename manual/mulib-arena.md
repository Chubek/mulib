# MuLIB arena

[Manuals](README.md) · [Library index](index.md)

Bump allocator with expandable allocation blocks. Pointer stability is guaranteed until reset. Individual allocations cannot be freed. Allocation aligns to max_align_t and cannot hold over-aligned types.

## Contract and behavior

Call `mulib_arena_init` on fresh storage; block size zero selects 4096
bytes. Allocation returns memory aligned for `max_align_t`, or NULL on overflow
or allocation failure. A zero-byte request still consumes one alignment unit.
Allocations remain valid across subsequent allocations. Release them together
with `mulib_arena_reset`, which frees every block and retains the configured
block size. There is no individual free or object destructor. Reinitializing a
live arena without resetting it leaks its blocks.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_ARENA
#include "mulib-arena.h"
int main(void) {
    mulib_arena_t a;
    mulib_arena_init(&a, 4096);
    int *x = mulib_arena_alloc(&a, sizeof(int));
    assert(x);
    *x = 9;
    mulib_arena_reset(&a);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-arena.h`](../include/mulib-arena.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-arena.h
 * @brief Bump allocator with expandable allocation blocks.
 * @defgroup mulib_arena MuLIB arena
 * @{
 * @details Pointer stability is guaranteed until reset. Individual allocations
 * cannot be freed. Allocation aligns to max_align_t and cannot hold
 * over-aligned types.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_ARENA before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_ARENA_H
#define MULIB_ARENA_H
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

  /** @brief Bump allocator with expandable allocation blocks. */
  typedef struct mulib_arena_block
  {
    struct mulib_arena_block *next;
    size_t used, capacity;
    max_align_t align;
    unsigned char data[1];
  } mulib_arena_block_t;
  /** @brief Bump allocator with expandable allocation blocks. */
  typedef struct mulib_arena
  {
    mulib_arena_block_t *head;
    size_t block_size;
  } mulib_arena_t;
  /** @brief mulib arena init; see manual/mulib-arena.md for contracts and
   * examples. */
  void mulib_arena_init (mulib_arena_t *a, size_t block_size);
  /** @brief mulib arena alloc; see manual/mulib-arena.md for contracts and
   * examples. */
  void *mulib_arena_alloc (mulib_arena_t *a, size_t size);
  /** @brief mulib arena reset; see manual/mulib-arena.md for contracts and
   * examples. */
  void mulib_arena_reset (mulib_arena_t *a);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ARENA_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-arena.h`](../include/mulib-arena.h) |
| Implementation switch | `MULIB_IMPL_ARENA` |
| Repository-local includes | None |
| Complete example | [`examples/modules/arena.c`](../examples/modules/arena.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/arena.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
