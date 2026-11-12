/* Complete, offline browser-limits example. */
#define MULIB_IMPL_BROWSER_LIMITS
#include "mulib-browser-limits.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_browser_limits_t limits;
  assert (mulib_browser_limits_default (&limits));
  assert (mulib_browser_limits_valid (&limits) && limits.max_depth == 128);
  return 0;
}
