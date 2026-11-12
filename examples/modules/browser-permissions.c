/* Complete, offline browser-permissions example. */
#define MULIB_IMPL_BROWSER_PERMISSIONS
#include "mulib-browser-permissions.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_browser_permissions_t p = { 0 };
  assert (mulib_browser_permissions_grant (&p, MULIB_BROWSER_ALLOW_NETWORK));
  assert (mulib_browser_permissions_check (p, MULIB_BROWSER_ALLOW_NETWORK));
  mulib_browser_permissions_revoke (&p, MULIB_BROWSER_ALLOW_NETWORK);
  return 0;
}
