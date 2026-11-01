#define MULIB_IMPL_LINALG
#define MULIB_IMPL_SIMDMATH
#define MULIB_IMPL_CGEOM
#define MULIB_IMPL_COLLISION
#include "mulib-collision.h"
#include <assert.h>
int
main (void)
{
  mulib_cgeom_sphere_t sphere = { { 0, 0, 0 }, 1 };
  mulib_cgeom_ray_t ray = { { 0, 0, 3 }, { 0, 0, -1 } };
  mulib_collision_hit_t hit;
  assert (mulib_collision_ray_sphere (ray, sphere, 0, 100, &hit) == 1);
  assert (hit.t == 2 && hit.normal.z == 1);
  return 0;
}
