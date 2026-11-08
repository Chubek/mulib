# MuLIB grid

[Manuals](README.md) · [Library index](index.md)

Two-dimensional packed grid of uniform-sized cells. Cells are zero-filled initially and laid out in row-major order. The grid owns bytes, not individual object resources.

## Contract and behavior

Initialize with width, height, and nonzero byte stride. Storage is
zero-filled and row-major; cell `(x,y)` starts at `(y * width + x) * stride`.
Zero dimensions are accepted. Init returns 1 on success, 0 for size overflow,
invalid arguments, or allocation failure. `at()` returns a borrowed cell pointer
or NULL outside the grid. It remains valid until `free()`, which zeros state.
Free before reinitializing. Cell bytes have no automatic nested destructors;
choose a stride compatible with any typed alignment requirements.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_GRID
#include "mulib-grid.h"
int main(void) {
    mulib_grid_t g;
    assert(mulib_grid_init(&g, 3, 3, sizeof(int)));
    int *p = mulib_grid_at(&g, 1, 1);
    assert(p);
    *p = 2;
    mulib_grid_free(&g);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-grid.h`](../include/mulib-grid.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-grid.h
 * @brief Two-dimensional packed grid of uniform-sized cells.
 * @defgroup mulib_grid MuLIB grid
 * @{
 * @details Cells are zero-filled initially and laid out in row-major order.
 * The grid owns bytes, not individual object resources.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_GRID before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_GRID_H
#define MULIB_GRID_H
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

  /** @brief Two-dimensional packed grid of uniform-sized cells. */
  typedef struct mulib_grid
  {
    unsigned char *data;
    size_t width, height, stride;
  } mulib_grid_t;
  /** @brief mulib grid init; see manual/mulib-grid.md for contracts and
   * examples. */
  int mulib_grid_init (mulib_grid_t *g, size_t width, size_t height,
                       size_t stride);
  /** @brief mulib grid at; see manual/mulib-grid.md for contracts and
   * examples. */
  void *mulib_grid_at (mulib_grid_t *g, size_t x, size_t y);
  /** @brief mulib grid free; see manual/mulib-grid.md for contracts and
   * examples. */
  void mulib_grid_free (mulib_grid_t *g);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_GRID_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-grid.h`](../include/mulib-grid.h) |
| Implementation switch | `MULIB_IMPL_GRID` |
| Repository-local includes | None |
| Complete example | [`examples/modules/grid.c`](../examples/modules/grid.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/grid.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
