#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_ARCHML
#define MULIB_IMPL_EMITTER
#define MULIB_IMPL_UNIT
#define MULIB_IMPL_MCU
#include "mulib-mcu.h"
#include <assert.h>
int main (void)
{
  mulib_mcu_t unit = {0};
  mulib_emitter_t image;
  unsigned char bytes[2];
  assert (mulib_mcu_init (&unit, "avr32", NULL) == MULIB_ARCHML_OK);
  assert (!mulib_emitter_init (&image, bytes, sizeof (bytes), unit.order));
  assert (mulib_mcu_emit (&unit, &image, "nop", NULL, 0, NULL) == MULIB_ARCHML_OK);
  assert (image.size == 2 && bytes[0] == 0xd7 && bytes[1] == 3);
  mulib_mcu_free (&unit);
  return 0;
}
