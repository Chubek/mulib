#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_WEBCLIENT
#define MULIB_IMPL_TSL
#define MULIB_IMPL_HTTPS
#include "mulib-https.h"
#include <assert.h>
/* Deterministic TLS interface fixture only; a real application supplies TLS.
 */
typedef struct fixture
{
  int delivered, closed;
} fixture;
static int
handshake (void *ctx, int *verified)
{
  (void)ctx;
  *verified = 1;
  return 0;
}
static int
write_bytes (void *ctx, const void *p, size_t n, size_t *got)
{
  (void)ctx;
  (void)p;
  *got = n;
  return 0;
}
static int
read_bytes (void *ctx, void *p, size_t n, size_t *got)
{
  fixture *f = ctx;
  const char reply[] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
  *got = 0;
  if (f->delivered)
    return MULIB_TSL_EOF;
  assert (n >= sizeof reply - 1);
  memcpy (p, reply, sizeof reply - 1);
  *got = sizeof reply - 1;
  f->delivered = 1;
  return 0;
}
static void
close_backend (void *ctx)
{
  ++((fixture *)ctx)->closed;
}
static int
connect_backend (void *ctx, const mulib_uri_t *uri, mulib_tsl_t *tls)
{
  mulib_tsl_ops_t ops = { handshake, read_bytes, write_bytes, close_backend };
  assert (uri->port.length == 3 && !memcmp (uri->port.data, "443", 3));
  return mulib_tsl_init (tls, &ops, ctx);
}
int
main (void)
{
  fixture f = { 0 };
  mulib_http_message_t response = { 0 };
  assert (mulib_https_request ("https://example.test/", "GET", NULL, 0, NULL,
                               0, NULL, connect_backend, &f, &response)
          == MULIB_WEBCLIENT_OK);
  assert (response.status == 200 && response.body_length == 2
          && f.closed == 1);
  mulib_http_message_free (&response);
  return 0;
}
