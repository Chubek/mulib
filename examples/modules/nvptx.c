#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_ARCHML
#define MULIB_IMPL_EMITTER
#define MULIB_IMPL_UNIT
#define MULIB_IMPL_GPU
#define MULIB_IMPL_NVPTX
#include "mulib-nvptx.h"
#include <assert.h>
#include <string.h>
int main (void)
{
  mulib_nvptx_t unit = {0};
  mulib_emitter_t image;
  char text[64];
  const char *operands[] = {"%r0", "%r1", "%r2"};
  const char expected[] = "add.s32 %r0, %r1, %r2;\n";
  assert (mulib_nvptx_init (&unit));
  assert (!mulib_emitter_init (&image, text, sizeof (text), MULIB_EMITTER_LITTLE));
  assert (mulib_nvptx_emit (&unit, &image, "add.s32", operands, 3, NULL) == MULIB_ARCHML_OK);
  assert (image.size == sizeof (expected) - 1 && !memcmp (text, expected, image.size));
  mulib_nvptx_free (&unit);
  return 0;
}
