#define MULIB_IMPL_DRM
#define MULIB_IMPL_LINALG
#define MULIB_IMPL_GLX
#include "mulib-glx.h"
#include <assert.h>
int
main (void)
{
  mulib_glx_t *surface = mulib_glx_create (64, 64);
  mulib_glx_vertex_t a = { 8, 8, 0, { 1, 0, 0, 1 } },
                     b = { 56, 8, 0, { 0, 1, 0, 1 } },
                     c = { 32, 56, 0, { 0, 0, 1, 1 } };
  assert (surface);
  mulib_glx_clear (surface, 0xff101820);
  assert (mulib_glx_triangle (surface, a, b, c, NULL, NULL));
  assert (mulib_glx_pixels (surface)[20 * 64 + 32] != 0xff101820);
  mulib_glx_destroy (surface);
  return 0;
}
