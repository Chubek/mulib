#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_ARCHML
#define MULIB_IMPL_EMITTER
#define MULIB_IMPL_UNIT
#define MULIB_IMPL_MCU
#define MULIB_IMPL_XTENSA
#include "mulib-xtensa.h"
#include <assert.h>
int main (void)
{
  mulib_xtensa_t unit = {0};
  mulib_emitter_t image;
  unsigned char bytes[3];
  const uint64_t operands[] = {2,3,4};
  assert (mulib_xtensa_init (&unit));
  assert (!mulib_emitter_init (&image, bytes, sizeof (bytes), unit.order));
  assert (mulib_xtensa_emit (&unit, &image, "add", operands, 3, NULL) == MULIB_ARCHML_OK);
  assert (image.size == 3 && bytes[0] == 0x40 && bytes[1] == 0x23 && bytes[2] == 0x80);
  mulib_xtensa_free (&unit);
  return 0;
}
