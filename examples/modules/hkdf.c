#define MULIB_IMPL_ENTROPY
#define MULIB_IMPL_SHA
#define MULIB_IMPL_HKDF
#include "mulib-hkdf.h"
#include <assert.h>
int main(void)
{
  unsigned char secret[32],prk[32],key[16];
  assert(mulib_entropy(secret,32));
  assert(mulib_hkdf_extract(MULIB_SHA256,NULL,0,secret,32,prk,32));
  assert(mulib_hkdf_expand(MULIB_SHA256,prk,32,"example application key",23,key,16));
  mulib_secret_wipe(secret,32);mulib_secret_wipe(prk,32);mulib_secret_wipe(key,16);return 0;
}
