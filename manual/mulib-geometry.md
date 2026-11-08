# MuLIB geometry

[Manuals](README.md) · [Library index](index.md)

Basic planar point geometry including distances and orientation. The polygon area is signed: counterclockwise polygons have positive area. Geometric computations use double precision and do not provide adaptive exact predicates.

## Contract and behavior

Distance uses `hypot`; cross computes `(b-a) x (c-a)` in two dimensions.
Polygon area uses the signed shoelace sum, implicitly closing the final vertex
to the first. In ordinary Cartesian coordinates, counterclockwise vertices
give positive area and clockwise vertices give negative area. NULL input or
fewer than three vertices returns zero. Self-intersections produce algebraic
area, not polygon validation. Inputs are borrowed for the call; no allocation
or robust exact-arithmetic predicate is provided.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_GEOMETRY
#include "mulib-geometry.h"
int main(void) {
    mulib_geometry_point_t a = {0, 0}, b = {3, 4};
    assert(mulib_geometry_distance(a, b) == 5);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-geometry.h`](../include/mulib-geometry.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-geometry.h
 * @brief Basic planar point geometry including distances and orientation.
 * @defgroup mulib_geometry MuLIB geometry
 * @{
 * @details The polygon area is signed: counterclockwise polygons have positive
 * area. Geometric computations use double precision and do not provide
 * adaptive exact predicates.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_GEOMETRY before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_GEOMETRY_H
#define MULIB_GEOMETRY_H
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

  /** @brief Basic planar point geometry including distances and orientation.
   */
  typedef struct mulib_geometry_point
  {
    double x, y;
  } mulib_geometry_point_t;
  /** @brief mulib geometry distance; see manual/mulib-geometry.md for
   * contracts and examples. */
  double mulib_geometry_distance (mulib_geometry_point_t a,
                                  mulib_geometry_point_t b);
  /** @brief mulib geometry cross; see manual/mulib-geometry.md for contracts
   * and examples. */
  double mulib_geometry_cross (mulib_geometry_point_t a,
                               mulib_geometry_point_t b,
                               mulib_geometry_point_t c);
  /** @brief mulib geometry polygon area; see manual/mulib-geometry.md for
   * contracts and examples. */
  double mulib_geometry_polygon_area (const mulib_geometry_point_t *points,
                                      size_t n);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_GEOMETRY_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-geometry.h`](../include/mulib-geometry.h) |
| Implementation switch | `MULIB_IMPL_GEOMETRY` |
| Repository-local includes | None |
| Complete example | [`examples/modules/geometry.c`](../examples/modules/geometry.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/geometry.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
