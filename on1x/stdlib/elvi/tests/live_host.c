#include "live.h"

#include <assert.h>
#include <string.h>

static const char success[] =
  "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n"
  "Content-Type: text/html; charset=utf-8\r\n\r\na\0b";
static const char redirect[] =
  "HTTP/1.1 302 Found\r\nContent-Length: 0\r\n"
  "Location: https://other.example/document\r\n\r\n";
static const char *response;
static size_t remaining;
static int invalid_peer;
static int64_t approvals, connects, closes;

int64_t live_fixture_authorizations(void) { return approvals; }
int64_t live_fixture_connects(void) { return connects; }
int64_t live_fixture_closes(void) { return closes; }

static int handshake(void *context, int *verified)
{
  (void)context;
  *verified = 1;
  return MULIB_TSL_OK;
}

static int read_chunk(void *context, void *out, size_t cap, size_t *len)
{
  (void)context;
  *len = remaining < cap ? remaining : cap;
  if (!*len) return MULIB_TSL_EOF;
  memcpy(out, response, *len);
  response += *len;
  remaining -= *len;
  return MULIB_TSL_OK;
}

static int write_chunk(void *context, const void *in, size_t len, size_t *out)
{
  (void)context;
  (void)in;
  *out = len;
  return MULIB_TSL_OK;
}

static void close_transport(void *context)
{
  (void)context;
  ++closes;
}

static int connect_fixture(void *context, const mulib_uri_t *uri, mulib_tsl_t *tls)
{
  const mulib_tsl_ops_t ops = {handshake, read_chunk, write_chunk, close_transport};
  assert(uri->host.length == 11 && !memcmp(uri->host.data, "example.org", 11));
  ++connects;
  return mulib_tsl_init(tls, &ops, context);
}

static int peer(void *context, const mulib_tsl_t *tls, mulib_elvi_scrape_peer_t *result)
{
  (void)context;
  (void)tls;
  result->certificate_verified = 1;
  result->hostname_verified = !invalid_peer;
  result->tls_version = 0x0304;
  strcpy(result->hostname, "example.org");
  strcpy(result->alpn, "http/1.1");
  return 1;
}

int on1x_elvi_live_authorize_v1(const on1x_elvi_live_request_v1 *request,
                               on1x_elvi_live_grant_v1 *grant)
{
  static const mulib_elvi_scrape_https_t https = {connect_fixture, peer, &approvals};
  ++approvals;
  assert(request->max_body_bytes > 0 && request->max_body_bytes <= 20);
  if (strcmp(request->host, "example.org")) return 0;
  invalid_peer = 0;
  if (!strcmp(request->url, "https://example.org/document")) {
    response = success;
    remaining = sizeof success - 1;
  } else if (!strcmp(request->url, "https://example.org/redirect")) {
    response = redirect;
    remaining = sizeof redirect - 1;
  } else if (!strcmp(request->url, "https://example.org/unverified")) {
    response = success;
    remaining = sizeof success - 1;
    invalid_peer = 1;
  } else {
    return 0;
  }
  *grant = (on1x_elvi_live_grant_v1){.approved_host = "example.org", .https = &https};
  return 1;
}
