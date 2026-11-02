#define MULIB_IMPL_AUTH
#define MULIB_IMPL_BASE64
#define MULIB_IMPL_SECURITY
#include "mulib-auth.h"
#include <assert.h>
int
main (void)
{
  char credentials[100];
  size_t n;
  assert (mulib_auth_basic ("Basic dXNlcjpwYXNz", credentials,
                            sizeof (credentials), &n));
  assert (n == 9 && !strcmp (credentials, "user:pass"));
  assert (!strcmp (mulib_auth_bearer ("Bearer opaque-token"), "opaque-token"));
  return 0;
}
