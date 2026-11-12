/* Complete, offline crawlarchive example. */
#define MULIB_IMPL_CRAWLARCHIVE
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#include "mulib-crawlarchive.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_crawlarchive_t a = { 0 };
  a.max_body = 1024;
  assert (mulib_crawlarchive_add (&a, "https://example.com", 200, "hello", 5));
  assert (mulib_crawlarchive_find (&a, "https://example.com")->body.len == 5);
  mulib_crawlarchive_free (&a);
  return 0;
}
