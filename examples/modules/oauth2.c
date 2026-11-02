#define MULIB_IMPL_OAUTH2
#define MULIB_IMPL_AUTH
#define MULIB_IMPL_BASE64
#define MULIB_IMPL_SECURITY
#include "mulib-oauth2.h"
#include <assert.h>
int
main (void)
{
  /* Missing verifier must never authorize. Real applications supply a trusted
   * signature/introspection backend and session-bound, single-use state. */
  assert (!mulib_oauth2_authorize ("Bearer opaque", "read", NULL, NULL));
  assert (mulib_oauth2_state ("session-nonce", "session-nonce"));
  assert (!mulib_oauth2_state ("session-nonce", "wrong"));
  return 0;
}
