/* Complete, offline browser-sandbox example. */
#define MULIB_IMPL_BROWSER_LIMITS
#define MULIB_IMPL_BROWSER_SANDBOX
#include "mulib-browser-sandbox.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_browser_limits_t limits;
  mulib_browser_sandbox_t sandbox;
  assert (mulib_browser_limits_default (&limits));
  assert (mulib_browser_sandbox_init (&sandbox, &limits));
  assert (mulib_browser_sandbox_charge (&sandbox, 100, 2, 10, 1));
  mulib_browser_sandbox_cancel (&sandbox);
  assert (!mulib_browser_sandbox_charge (&sandbox, 1, 0, 0, 0));
  return 0;
}
