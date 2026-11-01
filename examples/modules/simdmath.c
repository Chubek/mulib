#define MULIB_IMPL_LINALG
#define MULIB_IMPL_SIMDMATH
#include "mulib-simdmath.h"
#include <assert.h>
#include <stdio.h>
int
main (void)
{
  mulib_linalg_vec3_t translation = { 2, 3, 4 };
  mulib_linalg_vec4_t vertices[]
      = { { 0, 0, 0, 1 }, { 1, 2, 3, 1 }, { 1, 0, 0, 0 } };
  assert (mulib_simdmath_transform_many (
      mulib_linalg_mat4_translation (translation), vertices, vertices, 3));
  assert (vertices[0].x == 2 && vertices[1].z == 7 && vertices[2].x == 1);
  puts (mulib_simdmath_backend ());
  return 0;
}
