/* Complete, offline browser-cache example. */
#define MULIB_IMPL_BROWSER_CACHE
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#include "mulib-browser-cache.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_browser_cache_t c = { 0 };
  c.max_bytes = 1024;
  c.max_entries = 4;
  assert (mulib_browser_cache_put (&c, "https://example.com", "tag", 100,
                                   "hello", 5));
  assert (mulib_browser_cache_get (&c, "https://example.com", 99));
  assert (mulib_browser_cache_prune (&c, 100) == 1);
  mulib_browser_cache_free (&c);
  return 0;
}
