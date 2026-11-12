/* Complete, offline browser-origin example. */
#define MULIB_IMPL_AST
#define MULIB_IMPL_BROWSER_ORIGIN
#define MULIB_IMPL_GLOB
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INTE6
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_URI
#include "mulib-browser-origin.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_browser_origin_t a, b;
  assert (mulib_browser_origin_parse ("HTTPS://EXAMPLE.COM.:443/path", &a));
  assert (mulib_browser_origin_parse ("https://example.com", &b));
  assert (mulib_browser_origin_equal (&a, &b));
  return 0;
}
