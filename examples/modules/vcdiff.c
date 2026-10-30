#define MULIB_IMPL_VCDIFF
#include "mulib-vcdiff.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  unsigned char delta[256], result[64];
  size_t n, used;
  assert (mulib_vcdiff_encode ("hello world", 11, "hello there", 11, delta,
                               sizeof delta, &n));
  assert (mulib_vcdiff_decode ("hello world", 11, delta, n, result,
                               sizeof result, &used)
          == 1);
  assert (used == 11 && !memcmp (result, "hello there", 11));
  return 0;
}
