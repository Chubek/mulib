#define MULIB_IMPL_AUTHCRYPTO
#define MULIB_IMPL_SECURITY
#define MULIB_IMPL_BASE64
#define MULIB_IMPL_AUTH
#define MULIB_IMPL_OAUTH2
#define MULIB_IMPL_OAUTH
#include "mulib-oauth.h"
#include <assert.h>
int main (void)
{
  char challenge[44];
  assert(mulib_oauth_pkce_challenge("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk",challenge,sizeof challenge));
  assert(!mulib_oauth_authorize("Bearer opaque","read",NULL,NULL));
  assert(mulib_oauth2_scope("read write","read"));
  return 0;
}
