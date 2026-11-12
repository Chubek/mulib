/* Complete, offline browser-realm example. */
#define MULIB_IMPL_AST
#define MULIB_IMPL_BROWSER_ORIGIN
#define MULIB_IMPL_BROWSER_REALM
#define MULIB_IMPL_GLOB
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INTE6
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_URI
#include "mulib-browser-realm.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_browser_realm_t a, b;
  assert (mulib_browser_realm_init (&a, 1, 10, "https://EXAMPLE.com", 0));
  assert (mulib_browser_realm_init (&b, 2, 10, "https://example.com:443", 0));
  assert (mulib_browser_realm_same_origin (&a, &b));
  return 0;
}
