/* Complete, offline websearch-engine example. */
#define MULIB_IMPL_AST
#define MULIB_IMPL_BROWSER_ORIGIN
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
#define MULIB_IMPL_WEBSEARCH_ENGINE
#include "mulib-websearch-engine.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_websearch_engine_t e = { 0 };
  e.limit = 10;
  assert (mulib_websearch_engine_add (&e, "https://example.com", "Example", .9,
                                      1));
  assert (
      mulib_websearch_engine_add (&e, "https://example.com", "Example", 1, 2));
  mulib_websearch_engine_sort (&e);
  assert (e.len == 1 && e.hits[0].providers == 3);
  mulib_websearch_engine_free (&e);
  return 0;
}
