#define MULIB_IMPL_LINALG
#define MULIB_IMPL_SIMDMATH
#define MULIB_IMPL_CGEOM
#include "mulib-cgeom.h"
#include <assert.h>
int
main (void)
{
  mulib_linalg_vec2_t points[]
      = { { 0, 0 }, { 2, 0 }, { 1, 1 }, { 2, 2 }, { 0, 2 }, { 0, 0 } },
      hull[6], query = { 1, 1 };
  size_t count = 0;
  assert (mulib_cgeom_convex_hull (points, 6, hull, 6, &count) && count == 4);
  assert (mulib_cgeom_polygon_area (hull, count) == 4);
  assert (mulib_cgeom_polygon_contains (hull, count, query) == 1);
  return 0;
}
