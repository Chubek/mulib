#define MULIB_IMPL_ENTROPY
#define MULIB_IMPL_AES_GCM
#include "mulib-aes-gcm.h"
#include <assert.h>
#include <string.h>
int main(void)
{
  unsigned char key[16],nonce[12],cipher[5],plain[5],tag[16];mulib_aes_gcm_t ctx;
  /* A fresh key is used once. Long-lived keys need managed unique nonces. */
  assert(mulib_entropy(key,16)&&mulib_entropy(nonce,12)&&mulib_aes_gcm_init(&ctx,key));
  assert(mulib_aes_gcm_seal(&ctx,nonce,"metadata",8,"hello",5,cipher,tag));
  assert(mulib_aes_gcm_open(&ctx,nonce,"metadata",8,cipher,5,tag,plain));
  assert(!memcmp(plain,"hello",5));tag[0]^=1;
  assert(!mulib_aes_gcm_open(&ctx,nonce,"metadata",8,cipher,5,tag,plain));
  mulib_aes_gcm_clear(&ctx);mulib_secret_wipe(key,16);return 0;
}
