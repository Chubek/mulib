/* This host implements the mandatory Elvi authorizer with socket-free fixtures.
 * The peer report is synthetic input, not real TLS authentication. */
#include "elvi/live.h"

#include <stdio.h>
#include <string.h>

struct route {
  const char *host, *url, *path, *html;
};

static const struct route sites[] = {
  {"probe.example", "https://probe.example/daily", "/daily",
   "<!doctype html><title>Daily &amp; notes</title><p class='count'>42</p>"},
  {"calendar.test", "https://calendar.test/events", "/events",
   "<!doctype html><title>Open studio</title><p>Sat 10am</p>"}
};

/* Single-threaded fixture host. Store only a route from the constant table,
 * never the caller's borrowed request, and reset after synchronous close. */
struct fixture {
  const struct route *route;
  char wire[512], request[1024];
  size_t wire_size, read, written;
  int connected;
};

static int handshake(void *context, int *verified)
{
  (void)context;
  *verified = 1;
  return MULIB_TSL_OK;
}

static int read_reply(void *context, void *out, size_t cap, size_t *count)
{
  struct fixture *f = context;
  size_t available = f->wire_size - f->read;
  *count = 0;
  if (!f->read) {
    char expected[256];
    const int n = snprintf(expected, sizeof expected, "GET %s HTTP/1.1\r\nHost: %s:443\r\n",
                           f->route->path, f->route->host);
    if (n < 0 || (size_t)n >= sizeof expected || strncmp(f->request, expected, (size_t)n) ||
        strstr(f->request, "\r\nCookie:"))
      return MULIB_TSL_EBACKEND;
  }
  if (!available) return MULIB_TSL_EOF;
  if (cap > 13) cap = 13;  /* Exercise incremental HTTP decoding. */
  *count = available < cap ? available : cap;
  memcpy(out, f->wire + f->read, *count);
  f->read += *count;
  return MULIB_TSL_OK;
}

static int write_request(void *context, const void *in, size_t size, size_t *count)
{
  struct fixture *f = context;
  if (size > 17) size = 17;
  if (size >= sizeof f->request - f->written) {
    *count = 0;
    return MULIB_TSL_EBACKEND;
  }
  memcpy(f->request + f->written, in, size);
  f->written += size;
  f->request[f->written] = '\0';
  *count = size;
  return MULIB_TSL_OK;
}

static void close_reply(void *context)
{
  *(struct fixture *)context = (struct fixture){0};
}

static int connect_fixture(void *context, const mulib_uri_t *uri, mulib_tsl_t *session)
{
  static const mulib_tsl_ops_t ops = {handshake, read_reply, write_request, close_reply};
  struct fixture *f = context;
  if (!f->route || f->connected) return 0;
  const size_t length = strlen(f->route->host);
  if (uri->host.length != length || memcmp(uri->host.data, f->route->host, length) ||
      !uri->has_port || uri->port.length != 3 || memcmp(uri->port.data, "443", 3) ||
      !mulib_tsl_init(session, &ops, f)) {
    close_reply(f);
    return 0;
  }
  f->connected = 1;
  return 1;
}

static int fixture_peer(void *context, const mulib_tsl_t *session,
                        mulib_elvi_scrape_peer_t *peer)
{
  struct fixture *f = context;
  if (session->context != context || !f->connected || !f->route) return 0;
  memcpy(peer->hostname, f->route->host, strlen(f->route->host) + 1);
  memcpy(peer->alpn, "http/1.1", sizeof "http/1.1");
  peer->tls_version = 0x0304;
  peer->certificate_verified = 1;
  peer->hostname_verified = 1;
  return 1;
}

int on1x_elvi_live_authorize_v1(const on1x_elvi_live_request_v1 *request,
                                on1x_elvi_live_grant_v1 *grant)
{
  static struct fixture f;
  static const mulib_elvi_scrape_https_t https = {connect_fixture, fixture_peer, &f};
  if (f.route || !request->max_body_bytes || request->max_body_bytes > 4096) return 0;
  for (size_t i = 0; i < sizeof sites / sizeof *sites; ++i) {
    const struct route *site = &sites[i];
    if (strcmp(request->host, site->host) || strcmp(request->url, site->url)) continue;
    const int n = snprintf(f.wire, sizeof f.wire,
        "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %zu\r\n\r\n%s", strlen(site->html), site->html);
    if (n < 0 || (size_t)n >= sizeof f.wire) return 0;
    f.route = site;
    f.wire_size = (size_t)n;
    *grant = (on1x_elvi_live_grant_v1){.approved_host = site->host, .https = &https};
    return 1;
  }
  return 0;
}
