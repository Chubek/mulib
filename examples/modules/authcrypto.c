#define MULIB_IMPL_AUTHCRYPTO
#include "mulib-authcrypto.h"
#include <assert.h>
int main (void)
{
  unsigned char digest[32];
  assert(mulib_authcrypto_hash(MULIB_AUTHCRYPTO_SHA256,"abc",3,digest,sizeof digest));
  assert(digest[0]==0xba && digest[31]==0xad);
  mulib_authcrypto_wipe(digest,sizeof digest);
  return 0;
}
