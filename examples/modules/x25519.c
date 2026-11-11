#define MULIB_IMPL_ENTROPY
#define MULIB_IMPL_X25519
#include "mulib-x25519.h"
#include <assert.h>
int main(void)
{
  unsigned char a[32],b[32],ap[32],bp[32],shared_a[32],shared_b[32];
  assert(mulib_entropy(a,32)&&mulib_entropy(b,32));
  assert(mulib_x25519_public(ap,a)&&mulib_x25519_public(bp,b));
  assert(mulib_x25519(shared_a,a,bp)&&mulib_x25519(shared_b,b,ap));
  assert(mulib_secret_equal(shared_a,shared_b,32));
  mulib_secret_wipe(a,32);mulib_secret_wipe(b,32);
  mulib_secret_wipe(shared_a,32);mulib_secret_wipe(shared_b,32);return 0;
}
