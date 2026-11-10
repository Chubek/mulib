#define MULIB_IMPL_MINHASH
#include "mulib-minhash.h"
#include <assert.h>
int
main (void)
{
  uint64_t a[64], b[64];
  mulib_minhash_init (a, 64);
  mulib_minhash_init (b, 64);
  mulib_minhash_add (a, 64, "apple", 5);
  mulib_minhash_add (a, 64, "pear", 4);
  mulib_minhash_add (b, 64, "pear", 4);
  mulib_minhash_add (b, 64, "apple", 5);
  assert (mulib_minhash_similarity (a, b, 64) == 1);
  return 0;
}
