/* Complete, offline browser-snapshot example. */
#define MULIB_IMPL_BROWSER_SNAPSHOT
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#include "mulib-browser-snapshot.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_browser_snapshot_t a = { 0 }, b = { 0 };
  assert (mulib_browser_snapshot_make (&a, "hello", 5, 1, 100));
  assert (mulib_browser_snapshot_make (&b, "hello", 5, 2, 100));
  assert (mulib_browser_snapshot_equal (&a, &b));
  mulib_browser_snapshot_free (&a);
  mulib_browser_snapshot_free (&b);
  return 0;
}
