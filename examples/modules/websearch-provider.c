/* Complete, offline websearch-provider example. */
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
#define MULIB_IMPL_WEBSEARCH_PROVIDER
#include "mulib-websearch-provider.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_websearch_provider_config_t p;
  mulib_shylock_buffer_t url = { 0 };
  assert (mulib_websearch_provider_init (&p, "demo",
                                         "https://example.com/search", 10));
  assert (mulib_websearch_provider_url (&p, "a & b", 0, &url));
  assert (strstr (url.data, "a+%26+b"));
  mulib_shylock_buffer_free (&url);
  return 0;
}
