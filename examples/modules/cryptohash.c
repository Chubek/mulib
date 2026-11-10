#define MULIB_IMPL_SHA
#define MULIB_IMPL_CRYPTOHASH
#include "mulib-cryptohash.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
  unsigned char digest[32];
  assert(mulib_blake3_intrinsics("hello world",11,digest,sizeof digest));
  for(size_t i=0;i<sizeof digest;++i) printf("%02x",digest[i]);
  putchar('\n'); return 0;
}
