#define MULIB_IMPL_SHA
#include "mulib-sha.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
  unsigned char digest[32]; mulib_sha_ctx ctx;
  assert(mulib_sha_init(&ctx,MULIB_SHA256));
  assert(mulib_sha_update(&ctx,"hello",5));
  assert(mulib_sha_update(&ctx," world",6));
  assert(mulib_sha_final(&ctx,digest,sizeof digest));
  for(size_t i=0;i<sizeof digest;++i) printf("%02x",digest[i]);
  putchar('\n'); return 0;
}
