#define MULIB_IMPL_LSH
#define MULIB_IMPL_MINHASH
#include "mulib-lsh.h"
#include "mulib-minhash.h"
#include <assert.h>
int
main (void)
{
  uint64_t signature[16];
  mulib_minhash_init (signature, 16);
  mulib_minhash_add (signature, 16, "token", 5);
  uint64_t key = mulib_lsh_band (signature, 0, 4);
  assert (key == mulib_lsh_band (signature, 0, 4));
  return 0;
}
