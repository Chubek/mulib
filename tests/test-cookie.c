#define MULIB_IMPL_COOKIE
#include "mulib-cookie.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  char n[32], v[32];
  assert (mulib_cookie_pair ("session=abc; Path=/", n, sizeof n, v, sizeof v));
  assert (!strcmp (n, "session"));
  assert (!strcmp (v, "abc"));
  return 0;
}
