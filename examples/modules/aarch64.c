#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_ARCHML
#define MULIB_IMPL_EMITTER
#define MULIB_IMPL_UNIT
#define MULIB_IMPL_MPU
#define MULIB_IMPL_AARCH64
#include "mulib-aarch64.h"
#include <assert.h>
int main (void)
{
  mulib_aarch64_t unit = {0};
  mulib_emitter_t image;
  unsigned char bytes[4];
  const uint64_t operands[] = {0,1,2};
  assert (mulib_aarch64_init (&unit));
  assert (!mulib_emitter_init (&image, bytes, sizeof (bytes), unit.order));
  assert (mulib_aarch64_emit (&unit, &image, "add64", operands, 3, NULL) == MULIB_ARCHML_OK);
  assert (image.size == 4 && bytes[0] == 0x20 && bytes[3] == 0x8b);
  mulib_aarch64_free (&unit);
  return 0;
}
