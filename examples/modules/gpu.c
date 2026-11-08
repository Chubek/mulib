#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_ARCHML
#define MULIB_IMPL_EMITTER
#define MULIB_IMPL_UNIT
#define MULIB_IMPL_GPU
#include "mulib-gpu.h"
#include <assert.h>
int main (void)
{
  mulib_gpu_t unit = {0};
  mulib_emitter_t image;
  unsigned char bytes[4];
  assert (mulib_gpu_init (&unit, "amdgpu", NULL) == MULIB_ARCHML_OK);
  assert (!mulib_emitter_init (&image, bytes, sizeof (bytes), unit.order));
  assert (mulib_gpu_emit (&unit, &image, "s_endpgm", NULL, 0, NULL) == MULIB_ARCHML_OK);
  assert (image.size == 4 && bytes[2] == 0x81 && bytes[3] == 0xbf);
  mulib_gpu_free (&unit);
  return 0;
}
