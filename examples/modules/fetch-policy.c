/* Complete, offline fetch-policy example. */
#define MULIB_IMPL_AST
#define MULIB_IMPL_BROWSER_ORIGIN
#define MULIB_IMPL_FETCH_POLICY
#define MULIB_IMPL_GLOB
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INTE6
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_URI
#include "mulib-fetch-policy.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_fetch_policy_t p = { 0 };
  p.allow_https = 1;
  assert (mulib_fetch_policy_check (&p, "https://example.com"));
  assert (!mulib_fetch_policy_check (&p, "https://127.0.0.1"));
  return 0;
}
