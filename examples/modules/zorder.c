#define MULIB_IMPL_ZORDER
#include "mulib-zorder.h"
#include <assert.h>
int
main (void)
{
  uint32_t x, y;
  uint64_t key = mulib_zorder_encode (123, 456);
  mulib_zorder_decode (key, &x, &y);
  assert (x == 123 && y == 456);
  return 0;
}
