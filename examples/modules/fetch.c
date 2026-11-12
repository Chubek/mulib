/* Complete, offline fetch example. */
#define MULIB_IMPL_AST
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
#include "mulib-fetch.h"
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
  mulib_fetch_client_t client = { 0 };
  client.policy.allow_https = 1;
  client.transport = transport;
  mulib_shylock_buffer_t body = { 0 };
  unsigned status;
  char type[128];
  assert (mulib_fetch_get (&client, "https://example.com", &body, &status,
                           type, sizeof type));
  assert (status == 200);
  mulib_shylock_buffer_free (&body);
  return 0;
}
