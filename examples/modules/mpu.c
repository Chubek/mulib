#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_ARCHML
#define MULIB_IMPL_EMITTER
#define MULIB_IMPL_UNIT
#define MULIB_IMPL_MPU
#include "mulib-mpu.h"
#include <assert.h>
int main (void)
{
  mulib_mpu_t unit = {0};
  mulib_archml_register_t reg;
  assert (mulib_mpu_init (&unit, "aarch64", NULL) == MULIB_ARCHML_OK);
  assert (mulib_mpu_register_info (&unit, "w0", &reg));
  assert (reg.bits == 32 && reg.encoding == 0);
  assert (mulib_mpu_find (&unit, "abi", "aapcs64"));
  mulib_mpu_free (&unit);
  return 0;
}
