/* Complete, offline browser-navigation example. */
#define MULIB_IMPL_BROWSER_NAVIGATION
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#include "mulib-browser-navigation.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_browser_navigation_t n = { 0 };
  assert (mulib_browser_navigation_visit (&n, "https://example.com/a"));
  assert (mulib_browser_navigation_visit (&n, "https://example.com/b"));
  assert (!strcmp (mulib_browser_navigation_move (&n, -1),
                   "https://example.com/a"));
  mulib_browser_navigation_free (&n);
  return 0;
}
