# MuLIB bounds

[Manuals](README.md) · [Library index](index.md)

Axis-aligned bounding boxes and intersection utilities. All boundaries are inclusive. Callers must provide normalized boxes with min coordinates less than or equal to max coordinates.

## Contract and behavior

Bounds are double-precision `(minx, miny, maxx, maxy)` values. Supply
ordered, finite endpoints. Containment includes all four edges; intersection
also includes boxes that only touch at an edge or corner. Union takes component-
wise minima and maxima. These helpers allocate nothing and do not normalize
reversed endpoints or provide an error channel for invalid floating-point input.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_BOUNDS
#include "mulib-bounds.h"
int main(void) {
    mulib_bounds_t a = {0, 0, 2, 2};
    assert(mulib_bounds_contains(a, 1, 1));
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-bounds.h`](../include/mulib-bounds.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-bounds.h
 * @brief Axis-aligned bounding boxes and intersection utilities.
 * @defgroup mulib_bounds MuLIB bounds
 * @{
 * @details All boundaries are inclusive. Callers must provide normalized boxes
 * with min coordinates less than or equal to max coordinates.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_BOUNDS before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_BOUNDS_H
#define MULIB_BOUNDS_H
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

  /** @brief Axis-aligned bounding boxes and intersection utilities. */
  typedef struct mulib_bounds
  {
    double minx, miny, maxx, maxy;
  } mulib_bounds_t;
  /** @brief mulib bounds contains; see manual/mulib-bounds.md for contracts
   * and examples. */
  int mulib_bounds_contains (mulib_bounds_t b, double x, double y);
  /** @brief mulib bounds intersects; see manual/mulib-bounds.md for contracts
   * and examples. */
  int mulib_bounds_intersects (mulib_bounds_t a, mulib_bounds_t b);
  /** @brief mulib bounds union; see manual/mulib-bounds.md for contracts and
   * examples. */
  mulib_bounds_t mulib_bounds_union (mulib_bounds_t a, mulib_bounds_t b);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_BOUNDS_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-bounds.h`](../include/mulib-bounds.h) |
| Implementation switch | `MULIB_IMPL_BOUNDS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/bounds.c`](../examples/modules/bounds.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bounds.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
