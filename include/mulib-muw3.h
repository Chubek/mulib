/** @file mulib-muw3.h
 * @brief Bounded HTTP/HTTPS client, native TLS 1.3, redirects and form encoding.
 * Define MULIB_IMPL_MUW3 and its TLS/HTTP dependencies; see the manual. */
#ifndef MULIB_MUW3_H
#define MULIB_MUW3_H
#include "mulib-https.h"
#include "mulib-tls.h"
#include "mulib-socket.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_muw3_result {
  MULIB_MUW3_OK = 1, MULIB_MUW3_INVALID = -1, MULIB_MUW3_LIMIT = -2,
  MULIB_MUW3_NOMEM = -3, MULIB_MUW3_TRANSPORT = -4,
  MULIB_MUW3_TLS_REQUIRED = -5, MULIB_MUW3_REDIRECT = -6
} mulib_muw3_result;
typedef struct mulib_muw3_options {
  mulib_webclient_options_t web;
  size_t max_url_bytes;
  unsigned max_redirects, timeout_ms;
  int allow_downgrade;
  const char *ca_file; /* borrowed PEM bundle; NULL selects system trust */
  const mulib_x509_store_t *trust; /* borrowed, overrides ca_file */
} mulib_muw3_options_t;
/** Value configuration; callbacks/user are borrowed. NULL http uses native
 * POSIX sockets, NULL tls uses MuLIB TLS 1.3. Independent clients may run concurrently. */
typedef struct mulib_muw3_client {
  mulib_muw3_options_t options;
  mulib_webclient_transport_fn http;
  mulib_https_connect_fn tls;
  void *http_user, *tls_user;
} mulib_muw3_client_t;
typedef struct mulib_muw3_request {
  const char *url, *method; /* NULL method selects GET */
  const mulib_http_header_t *headers;
  size_t header_count;
  const void *body;
  size_t body_length;
} mulib_muw3_request_t;
typedef struct mulib_muw3_response {
  mulib_http_message_t message;
  char *url; /* owned final URL, without fragment */
  unsigned redirects;
} mulib_muw3_response_t;
/** Defaults: 5 redirects, 8192 URL bytes, 30s per socket wait, chunk metadata.
 * A zero-initialized options struct instead disables redirects/metadata;
 * zero byte/time limits still select defaults. */
mulib_muw3_client_t mulib_muw3_client_default (void);
/** Synchronous transaction. HTTP error statuses are successful transactions.
 * Outputs unchanged on error; successful response must be freed before reuse.
 * Redirects resolve relative references, strip cross-origin Authorization/Cookie,
 * rewrite POST on 301/302 and non-HEAD on 303, and bound every hop. */
mulib_muw3_result mulib_muw3_perform (const mulib_muw3_client_t *,
    const mulib_muw3_request_t *, mulib_muw3_response_t *);
/** Live decoded response callbacks; no body retention, no redirect following.
 * On success out owns final URL, headers and trailers. Limits are cumulative.
 * Callback sees headers once (NULL,0), then body bytes; return 1 to continue. */
mulib_muw3_result mulib_muw3_stream (const mulib_muw3_client_t *,
    const mulib_muw3_request_t *, mulib_http_body_fn, void *, mulib_muw3_response_t *);
void mulib_muw3_response_free (mulib_muw3_response_t *);
const char *mulib_muw3_strerror (int result);
/** application/x-www-form-urlencoded text; output malloc-owned on success.
 * Same function can generate a query string (without leading '?'). */
mulib_muw3_result mulib_muw3_form (const mulib_http_header_t *fields, size_t count,
                                  size_t limit, char **out);
/** Native WEBCLIENT callback. user is an optional unsigned timeout_ms pointer.
 * Uses system DNS (its own timeout), nonblocking TCP, bounded I/O waits.
 * Define _POSIX_C_SOURCE=200809L before system headers to enable DNS. */
int mulib_muw3_transport (void *, const mulib_uri_t *, const void *, size_t,
                         mulib_webclient_receive_fn, void *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MUW3) && !defined(MULIB_MUW3_IMPLEMENTED)
#define MULIB_MUW3_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static int mulib_muw3_equal_ (const char *a, size_t n, const char *b, size_t m)
{
  if (n != m) return 0;
  for (size_t i = 0; i < n; ++i) {
    unsigned char x = (unsigned char)a[i], y = (unsigned char)b[i];
    if (x >= 'A' && x <= 'Z') x += 32;
    if (y >= 'A' && y <= 'Z') y += 32;
    if (x != y) return 0;
  }
  return 1;
}
static int mulib_muw3_name_ (const char *a, const char *b)
{ return a && mulib_muw3_equal_ (a, strlen (a), b, strlen (b)); }
static int mulib_muw3_endpoint_ (const char *s, size_t max, mulib_uri_t *u)
{
  size_t n = 0;
  unsigned port = 0;
  if (!s) return 0;
  while (n < max && s[n]) ++n;
  if (s[n] || !mulib_uri_parse (s, n, u) || !u->has_authority || !u->host.length
      || u->has_userinfo || (u->has_port && !u->port.length)) return 0;
  if (!mulib_muw3_equal_ (u->scheme.data, u->scheme.length, "http", 4)
      && !mulib_muw3_equal_ (u->scheme.data, u->scheme.length, "https", 5)) return 0;
  if (u->has_port) {
    for (size_t i = 0; i < u->port.length; ++i) {
      unsigned d = (unsigned char)u->port.data[i] - (unsigned)'0';
      if (d > 9 || port > (65535u - d) / 10u) return 0;
      port = port * 10 + d;
    }
    if (!port) return 0;
  }
  return 1;
}
static unsigned mulib_muw3_port_ (const mulib_uri_t *u)
{
  unsigned p = 0;
  if (!u->has_port) return u->scheme.length == 5 ? 443 : 80;
  for (size_t i = 0; i < u->port.length; ++i) p = p * 10 + (unsigned)(u->port.data[i] - '0');
  return p;
}
mulib_muw3_client_t mulib_muw3_client_default (void)
{
  mulib_muw3_client_t c;
  memset (&c, 0, sizeof c);
  c.options.max_redirects = 5; c.options.max_url_bytes = 8192;
  c.options.timeout_ms = 30000; c.options.web.http.flags = MULIB_HTTP_CHUNK_METADATA;
  return c;
}
void mulib_muw3_response_free (mulib_muw3_response_t *r)
{
  if (r) { mulib_http_message_free (&r->message); free (r->url); memset (r, 0, sizeof *r); }
}
const char *mulib_muw3_strerror (int r)
{
  switch (r) {
  case 1: return "success";
  case -1: return "invalid HTTP request or response";
  case -2: return "HTTP resource limit";
  case -3: return "allocation failed";
  case -4: return "transport failure or timeout";
  case -5: return "verified TLS connector required";
  case -6: return "redirect limit or disallowed redirect";
  default: return "unknown MuW3 result";
  }
}
int mulib_muw3_transport (void *user, const mulib_uri_t *u, const void *wire, size_t length,
    mulib_webclient_receive_fn receive, void *receiver)
{
  char host[256];
  unsigned char data[8192];
  mulib_socket_t s = MULIB_SOCKET_INIT;
  size_t n, start = 0, off = 0;
  unsigned timeout = user ? *(const unsigned *)user : 30000;
  int ok = 0;
  if (!u || !receive || (!wire && length)
      || !mulib_muw3_equal_ (u->scheme.data, u->scheme.length, "http", 4)) return 0;
  n = u->host.length;
  if (n > 2 && u->host.data[0] == '[') { start = 1; n -= 2; }
  if (!n || n >= sizeof host) return 0;
  memcpy (host, u->host.data + start, n); host[n] = 0;
  if (!timeout) timeout = 30000;
  if (mulib_socket_connect_host (&s, host, mulib_muw3_port_ (u), timeout)) return 0;
  while (off < length) {
    int rc;
    if (mulib_socket_wait (&s, 1, timeout)) goto done;
    rc = mulib_socket_send (&s, (const unsigned char *)wire + off, length - off, &n);
    if (rc == MULIB_SOCKET_AGAIN) continue;
    if (rc || !n) goto done;
    off += n;
  }
  for (;;) {
    int rc;
    if (mulib_socket_wait (&s, 0, timeout)) break;
    rc = mulib_socket_receive (&s, data, sizeof data, &n);
    if (rc == MULIB_SOCKET_AGAIN) continue;
    if (rc == MULIB_SOCKET_EOF) { ok = 1; break; }
    if (rc || !receive (receiver, data, n)) break;
  }
done:
  (void)mulib_socket_close (&s); return ok;
}
static mulib_muw3_result mulib_muw3_perform_ (const mulib_muw3_client_t *client,
    const mulib_muw3_request_t *request, mulib_muw3_response_t *out,
    mulib_http_body_fn callback, void *callback_user)
{
  mulib_muw3_client_t defaults = mulib_muw3_client_default ();
  const mulib_muw3_client_t *c = client ? client : &defaults;
  mulib_muw3_response_t r = {0};
  mulib_uri_t u, next, ref;
  mulib_http_header_t *headers = NULL;
  const char *method;
  const void *body;
  size_t count, length, max = c->options.max_url_bytes ? c->options.max_url_bytes : 8192;
  mulib_muw3_result rc = MULIB_MUW3_INVALID;
  if (!request || !out || (!request->headers && request->header_count)
      || (!request->body && request->body_length) || !max || max > 1048576
      || c->options.max_redirects > 100 || (c->options.allow_downgrade != 0 && c->options.allow_downgrade != 1)
      || !mulib_muw3_endpoint_ (request->url, max, &u)) return rc;
  count = request->header_count;
  if (count > (c->options.web.http.max_headers ? c->options.web.http.max_headers : 100)
      || count > SIZE_MAX / sizeof (*headers)) return MULIB_MUW3_LIMIT;
  for (size_t i = 0; i < count; ++i)
    if (!request->headers[i].name || !request->headers[i].value) return rc;
  u.has_fragment = 0; u.fragment.length = 0;
  r.url = mulib_uri_compose (&u);
  if (!r.url) return MULIB_MUW3_NOMEM;
  if (count) {
    headers = (mulib_http_header_t *)malloc (count * sizeof (*headers));
    if (!headers) { mulib_muw3_response_free (&r); return MULIB_MUW3_NOMEM; }
    memcpy (headers, request->headers, count * sizeof (*headers));
  }
  method = request->method ? request->method : "GET";
  body = request->body; length = request->body_length;
  for (;;) {
    const char *location;
    char *url;
    int cross, drop;
    if (!mulib_muw3_endpoint_ (r.url, max, &u)) { rc = MULIB_MUW3_INVALID; break; }
    if (u.scheme.length == 5) {
      mulib_tls_options_t tls_options;
      tls_options.trust = c->options.trust; tls_options.ca_file = c->options.ca_file;
      tls_options.timeout_ms = c->options.timeout_ms;
      if (callback) rc = (mulib_muw3_result)mulib_https_stream_request (r.url, method,
          headers, count, body, length, &c->options.web,
          c->tls ? c->tls : mulib_tls_connect, c->tls ? c->tls_user : &tls_options,
          callback, callback_user, &r.message);
      else rc = (mulib_muw3_result)mulib_https_request (r.url, method, headers, count, body, length,
          &c->options.web, c->tls ? c->tls : mulib_tls_connect,
          c->tls ? c->tls_user : &tls_options, &r.message);
    } else if (callback) rc = (mulib_muw3_result)mulib_webclient_stream_request (r.url, method,
        headers, count, body, length, &c->options.web,
        c->http ? c->http : mulib_muw3_transport,
        c->http ? c->http_user : (void *)&c->options.timeout_ms,
        callback, callback_user, &r.message);
    else rc = (mulib_muw3_result)mulib_webclient_request (r.url, method, headers, count, body,
        length, &c->options.web, c->http ? c->http : mulib_muw3_transport,
        c->http ? c->http_user : (void *)&c->options.timeout_ms, &r.message);
    if (rc != MULIB_MUW3_OK) break;
    location = mulib_http_header (&r.message, "Location");
    if (callback || !c->options.max_redirects || !location || (r.message.status != 301 && r.message.status != 302
        && r.message.status != 303 && r.message.status != 307 && r.message.status != 308)) {
      *out = r; free (headers); return MULIB_MUW3_OK;
    }
    if (r.redirects == c->options.max_redirects) { rc = MULIB_MUW3_REDIRECT; break; }
    if (strlen (location) > max || !mulib_uri_parse (location, strlen (location), &ref)) {
      rc = MULIB_MUW3_REDIRECT; break;
    }
    ref.has_fragment = 0; ref.fragment.length = 0;
    url = mulib_uri_resolve (&u, &ref);
    if (!url) { rc = MULIB_MUW3_NOMEM; break; }
    if (!mulib_muw3_endpoint_ (url, max, &next)
        || (!c->options.allow_downgrade && u.scheme.length == 5 && next.scheme.length == 4)) {
      free (url); rc = MULIB_MUW3_REDIRECT; break;
    }
    cross = u.scheme.length != next.scheme.length || mulib_muw3_port_ (&u) != mulib_muw3_port_ (&next)
        || !mulib_muw3_equal_ (u.host.data, u.host.length, next.host.data, next.host.length);
    drop = (r.message.status == 303 && strcmp (method, "HEAD"))
        || ((r.message.status == 301 || r.message.status == 302) && !strcmp (method, "POST"));
    if (drop) { method = "GET"; body = NULL; length = 0; }
    {
      size_t j = 0;
      for (size_t i = 0; i < count; ++i) {
        const char *name = headers[i].name;
        if (cross && (mulib_muw3_name_ (name, "Authorization") || mulib_muw3_name_ (name, "Cookie")
            || mulib_muw3_name_ (name, "Proxy-Authorization"))) continue;
        if (drop && (mulib_muw3_name_ (name, "Content-Length") || mulib_muw3_name_ (name, "Transfer-Encoding")
            || mulib_muw3_name_ (name, "Content-Type") || mulib_muw3_name_ (name, "Content-Encoding")
            || mulib_muw3_name_ (name, "Expect"))) continue;
        headers[j++] = headers[i];
      }
      count = j;
    }
    free (r.url); r.url = url; ++r.redirects; mulib_http_message_free (&r.message);
  }
  free (headers); mulib_muw3_response_free (&r); return rc;
}
mulib_muw3_result mulib_muw3_perform (const mulib_muw3_client_t *c,
    const mulib_muw3_request_t *q, mulib_muw3_response_t *r)
{ return mulib_muw3_perform_ (c, q, r, NULL, NULL); }
mulib_muw3_result mulib_muw3_stream (const mulib_muw3_client_t *c,
    const mulib_muw3_request_t *q, mulib_http_body_fn cb, void *u, mulib_muw3_response_t *r)
{
  if (!cb) return MULIB_MUW3_INVALID;
  return mulib_muw3_perform_ (c, q, r, cb, u);
}
mulib_muw3_result mulib_muw3_form (const mulib_http_header_t *fields, size_t count,
    size_t limit, char **out)
{
  static const char hex[] = "0123456789ABCDEF";
  mulib_strbuf_t b = {0};
  int rc = MULIB_MUW3_NOMEM;
  if (!out || (!fields && count)) return MULIB_MUW3_INVALID;
  if (!limit) limit = 1048576;
  if (count > limit / 2 + 1) return MULIB_MUW3_LIMIT;
  for (size_t i = 0; i < count; ++i) {
    if (!fields[i].name || !fields[i].value) { rc = MULIB_MUW3_INVALID; goto done; }
    for (unsigned j = 0; j < 2; ++j) {
      const unsigned char *p = (const unsigned char *)(j ? fields[i].value : fields[i].name);
      if (j || i) {
        if (b.length == limit) { rc = MULIB_MUW3_LIMIT; goto done; }
        if (!mulib_strbuf_putc (&b, j ? '=' : '&')) goto done;
      }
      for (; *p; ++p) {
        char encoded[3]; size_t n = 1;
        unsigned char x = *p;
        encoded[0] = (char)x;
        if (x == ' ') encoded[0] = '+';
        else if (!((x >= 'a' && x <= 'z') || (x >= 'A' && x <= 'Z') || (x >= '0' && x <= '9')
            || x == '-' || x == '_' || x == '.' || x == '*')) {
          encoded[0] = '%'; encoded[1] = hex[x >> 4]; encoded[2] = hex[x & 15]; n = 3;
        }
        if (n > limit - b.length) { rc = MULIB_MUW3_LIMIT; goto done; }
        if (!mulib_strbuf_append_n (&b, encoded, n)) goto done;
      }
    }
  }
  if (!mulib_strbuf_append (&b, "")) goto done;
  *out = b.data; return MULIB_MUW3_OK;
done:
  mulib_strbuf_free (&b); return (mulib_muw3_result)rc;
}
#endif
