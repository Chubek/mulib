#define MULIB_IMPL_LINALG
#include "mulib-linalg.h"
#include <assert.h>
#include <math.h>
int
main (void)
{
  mulib_linalg_vec3_t eye = { 0, 0, 5 }, target = { 0, 0, 0 },
                      up = { 0, 1, 0 };
  mulib_linalg_mat4_t view, projection, mvp, inverse;
  mulib_linalg_vec4_t point = { 1, 1, 0, 1 }, clip;
  assert (mulib_linalg_look_at (eye, target, up, &view));
  assert (mulib_linalg_perspective (1.2f, 16.0f / 9, 0.1f, 100, &projection));
  mvp = mulib_linalg_mat4_mul (projection, view);
  assert (mulib_linalg_mat4_inverse (mvp, &inverse));
  clip = mulib_linalg_mat4_transform (mvp, point);
  point = mulib_linalg_mat4_transform (inverse, clip);
  assert (fabsf (point.x / point.w - 1) < 0.0001f);
  return 0;
}
