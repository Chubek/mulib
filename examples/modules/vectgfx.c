#define MULIB_IMPL_DRM
#define MULIB_IMPL_LINALG
#define MULIB_IMPL_GLX
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_VEC
#define MULIB_IMPL_VECTGFX
#include "mulib-vectgfx.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  const char *source = "M 8 48 C 8 4 56 4 56 48 L 8 48 Z";
  mulib_vectgfx_t *path = mulib_vectgfx_parse (source, strlen (source), NULL);
  mulib_glx_t *surface = mulib_glx_create (64, 64);
  assert (path && surface);
  assert (mulib_vectgfx_fill (path, surface, 0xff3489db, 0.25f));
  assert (mulib_vectgfx_stroke (path, surface, 0xffffffff, 2, 0.25f));
  assert (mulib_glx_pixels (surface)[32 * 64 + 32] == 0xff3489db);
  mulib_vectgfx_destroy (path);
  mulib_glx_destroy (surface);
  return 0;
}
