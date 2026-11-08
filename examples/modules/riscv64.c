#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_ARCHML
#define MULIB_IMPL_EMITTER
#define MULIB_IMPL_UNIT
#define MULIB_IMPL_MPU
#define MULIB_IMPL_RISCV64
#include "mulib-riscv64.h"
#include <assert.h>
int main (void)
{
  mulib_riscv64_t unit = {0};
  unsigned char bytes[4];
  size_t length = 0;
  const uint64_t operands[] = {10,11,12};
  uint64_t decoded[3];
  assert (mulib_riscv64_init (&unit));
  assert (mulib_riscv64_encode (&unit, "add", operands, 3, bytes, sizeof (bytes), &length) == MULIB_ARCHML_OK);
  assert (mulib_riscv64_decode (&unit, "add", bytes, length, decoded, 3, &length) == MULIB_ARCHML_OK);
  assert (length == 4 && decoded[0] == 10 && decoded[2] == 12);
  mulib_riscv64_free (&unit);
  return 0;
}
