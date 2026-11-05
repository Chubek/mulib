#define MULIB_IMPL_2DGEOM
#define MULIB_IMPL_GEOMETRY
#define MULIB_IMPL_VEC
#define MULIB_IMPL_PARSEC
#include "mulib-2dgeom.h"
#include <assert.h>
int main(void) {
  mulib_2dgeom_path_t path = {0}; mulib_2dgeom_error_t error;
  mulib_2dgeom_point_t point, tangent; mulib_2dgeom_curve_t left, right;
  const char text[] = "M0 0 C0 1 1 1 1 0";
  assert(mulib_2dgeom_path_parse(text, sizeof text - 1, &path, &error));
  assert(mulib_2dgeom_evaluate(&mulib_2dgeom_path_at(&path, 0)->curve, 0.5, &point));
  assert(point.x == 0.5 && point.y == 0.75);
  assert(mulib_2dgeom_tangent(&mulib_2dgeom_path_at(&path, 0)->curve, 0.5, &tangent) && tangent.x == 1);
  assert(mulib_2dgeom_split(&mulib_2dgeom_path_at(&path, 0)->curve, 0.5, &left, &right));
  mulib_2dgeom_path_free(&path); return 0;
}
