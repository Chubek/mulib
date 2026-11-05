#define MULIB_IMPL_AES
#define MULIB_IMPL_AUTHCRYPTO
#include "mulib-aes.h"
#include <assert.h>
#include <string.h>
int main(void) {
  unsigned char key[64]={0}, nonce[12]={0}, cipher[5], plain[5], tag[32];
  /* Example-only key/nonce: provision secret keys and unique nonces in production. */
  assert(mulib_aes_seal(key,nonce,"header",6,"hello",5,cipher,tag));
  assert(mulib_aes_open(key,nonce,"header",6,cipher,5,tag,plain));
  assert(!memcmp(plain,"hello",5));
  tag[0]^=1; assert(!mulib_aes_open(key,nonce,"header",6,cipher,5,tag,plain));
  return 0;
}
