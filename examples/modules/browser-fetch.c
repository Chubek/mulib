/* Complete, offline browser-fetch example. */
#define MULIB_IMPL_AST
#define MULIB_IMPL_BROWSER_FETCH
#define MULIB_IMPL_BROWSER_ORIGIN
#define MULIB_IMPL_FETCH
#define MULIB_IMPL_FETCH_POLICY
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
#include "mulib-browser-fetch.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static int
transport (void *ctx, const char *url, mulib_shylock_buffer_t *body,
           unsigned *status, char *type, size_t cap)
{
  (void)ctx;
  (void)url;
  *status = 200;
  return mulib_shylock_copy (type, cap, "text/html")
         && mulib_shylock_text (body, "<p>Hello</p>");
}

int
main (void)
{
  mulib_browser_fetch_t f = { 0 };
  f.client.policy.allow_https = 1;
  f.client.transport = transport;
  assert (mulib_browser_origin_parse ("https://example.com", &f.initiator));
  f.enforce_same_origin = 1;

  mulib_shylock_buffer_t body = { 0 };
  unsigned status;
  char type[128];
  assert (mulib_browser_fetch_get (&f, "https://example.com", &body, &status,
                                   type, sizeof type));
  mulib_shylock_buffer_free (&body);
  return 0;
}
