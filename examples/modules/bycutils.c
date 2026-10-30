#define MULIB_IMPL_BYCUTILS
#include "mulib-bycutils.h"
#include <assert.h>
int
main (void)
{
  unsigned char b[8];
  uint64_t v;
  size_t aligned;
  assert (mulib_bycutils_store (b, 8, 1, 4, 0x12345678));
  assert (mulib_bycutils_load (b, 8, 1, 4, &v) && v == 0x12345678);
  assert (mulib_bycutils_align (17, 16, &aligned) && aligned == 32);
  return 0;
}
