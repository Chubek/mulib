#define MULIB_IMPL_OAUTH2
#define MULIB_IMPL_AUTHCRYPTO
#define MULIB_IMPL_AUTH
#define MULIB_IMPL_BASE64
#define MULIB_IMPL_SECURITY
#include "mulib-oauth2.h"
#include <assert.h>
int
main (void)
{
  char challenge[44];
  mulib_oauth2_transaction_t transaction={"session-nonce",100,0};
  /* Missing verifier must never authorize. Real applications supply a trusted
   * signature/introspection backend and session-bound, single-use state. */
  assert (!mulib_oauth2_authorize ("Bearer opaque", "read", NULL, NULL));
  assert (mulib_oauth2_state ("session-nonce", "session-nonce"));
  assert (!mulib_oauth2_state ("session-nonce", "wrong"));
  assert (mulib_oauth2_state_consume(&transaction,"session-nonce",99));
  assert (!mulib_oauth2_state_consume(&transaction,"session-nonce",99));
  assert (mulib_oauth2_pkce_challenge("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk",challenge,sizeof challenge));
  assert (mulib_oauth2_pkce_verify("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk",challenge));
  return 0;
}
