#define MULIB_IMPL_SIMHASH
#include "mulib-simhash.h"
#include <assert.h>
int
main (void)
{
  uint64_t tokens[] = { 0x0f, 0x07 };
  double weights[] = { 2, 1 };
  uint64_t fingerprint = mulib_simhash64 (tokens, weights, 2);
  assert (fingerprint == 0x0f && mulib_simhash_distance (fingerprint, 0x07) == 1);
  return 0;
}
