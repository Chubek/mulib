#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_ARCHML
#define MULIB_IMPL_EMITTER
#define MULIB_IMPL_UNIT
#define MULIB_IMPL_MCU
#define MULIB_IMPL_AVR32
#include "mulib-avr32.h"
#include <assert.h>
int main (void)
{
  mulib_avr32_t unit = {0};
  mulib_emitter_t image;
  unsigned char bytes[2];
  const uint64_t operands[] = {2,3};
  assert (mulib_avr32_init (&unit));
  assert (!mulib_emitter_init (&image, bytes, sizeof (bytes), MULIB_EMITTER_BIG));
  assert (mulib_avr32_emit (&unit, &image, "add", operands, 2, NULL) == MULIB_ARCHML_OK);
  assert (image.size == 2 && bytes[0] == 6 && bytes[1] == 2);
  mulib_avr32_free (&unit);
  return 0;
}
