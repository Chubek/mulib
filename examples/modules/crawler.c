/* Complete, offline crawler example. */
#define MULIB_IMPL_AST
#define MULIB_IMPL_BROWSER_ORIGIN
#define MULIB_IMPL_CRAWLER
#define MULIB_IMPL_GLOB
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INTE6
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_URI
#define MULIB_IMPL_UTF8
#include "mulib-crawler.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_crawler_t c = { 0 };
  size_t id;
  assert (mulib_crawler_enqueue (&c, "https://example.com", 10));
  assert (mulib_crawler_next (&c, 10, &id));
  assert (mulib_crawler_finish (&c, id, 1, 0, 3));
  mulib_crawler_free (&c);
  return 0;
}
