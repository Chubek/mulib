#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_ARCHML
#define MULIB_IMPL_EMITTER
#define MULIB_IMPL_UNIT
#define MULIB_IMPL_GPU
#define MULIB_IMPL_AMDGPU
#include "mulib-amdgpu.h"
#include <assert.h>
int main (void)
{
  mulib_amdgpu_t unit = {0};
  mulib_emitter_t image;
  unsigned char bytes[4];
  const uint64_t operands[] = {0,1,2};
  assert (mulib_amdgpu_init (&unit));
  assert (!mulib_emitter_init (&image, bytes, sizeof (bytes), unit.order));
  assert (mulib_amdgpu_emit (&unit, &image, "s_add_u32", operands, 3, NULL) == MULIB_ARCHML_OK);
  assert (image.size == 4 && bytes[0] == 1 && bytes[1] == 2 && bytes[3] == 0x80);
  mulib_amdgpu_free (&unit);
  return 0;
}
