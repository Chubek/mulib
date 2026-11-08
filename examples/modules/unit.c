#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_ARCHML
#define MULIB_IMPL_EMITTER
#define MULIB_IMPL_UNIT
#include "mulib-unit.h"
#include <assert.h>
int main (void)
{
  mulib_unit_t unit = {0};
  unsigned char bytes[4];
  size_t length = 0;
  const uint64_t operands[] = {10,11,12};
  assert (mulib_unit_init (&unit, MULIB_UNIT_MPU, "riscv64", NULL) == MULIB_ARCHML_OK);
  assert (mulib_unit_encode (&unit, "add", operands, 3, bytes, sizeof (bytes), &length) == MULIB_ARCHML_OK);
  assert (length == 4 && bytes[0] == 0x33 && bytes[1] == 0x85);
  mulib_unit_free (&unit);
  return 0;
}
