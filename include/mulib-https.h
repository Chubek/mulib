/** @file mulib-https.h
 * @brief HTTPS transactions composing HTTP webclient with verified TLS.
 * Define MULIB_IMPL_HTTPS and all WEBCLIENT/TSL dependencies once.
 */
#ifndef MULIB_HTTPS_H
#define MULIB_HTTPS_H
#include "mulib-tsl.h"
#include "mulib-webclient.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Create a TLS session in zero-state output, returning 1 success.
   * Endpoint is the original https URI with effective port (default 443).
   * Backend must verify its host/chain and negotiate HTTP/1.1 (not HTTP/2).
   * On return, adapter owns close even on failure if state became nonzero.
   * This synchronous API requires blocking backend callbacks; AGAIN aborts.
   */
  typedef int (*mulib_https_connect_fn) (void *, const mulib_uri_t *,
                                         mulib_tsl_t *);
  /** @brief Same message/limits ownership as webclient. Only https URLs; every
   * request uses a fresh verified TLS session, closed on every path. No
   * redirects or TLS implementation is implicit. Returns
   * mulib_webclient_result. */
  mulib_webclient_result
  mulib_https_request (const char *url, const char *method,
                       const mulib_http_header_t *, size_t, const void *,
                       size_t, const mulib_webclient_options_t *,
                       mulib_https_connect_fn, void *, mulib_http_message_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HTTPS) && !defined(MULIB_HTTPS_IMPLEMENTED)
#define MULIB_HTTPS_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
typedef struct mulib_https_context_
{
  mulib_uri_t endpoint;
  mulib_https_connect_fn connect;
  void *user;
} mulib_https_context_;
static int
mulib_https_transport_ (void *ctx, const mulib_uri_t *unused,
                        const void *request, size_t n,
                        mulib_webclient_receive_fn receive, void *receiver)
{
  mulib_https_context_ *c = ctx;
  mulib_tsl_t s;
  unsigned char buffer[8192];
  size_t off = 0;
  int ok = 0;
  (void)unused;
  memset (&s, 0, sizeof (s));
  if (c->connect (c->user, &c->endpoint, &s) != 1
      || mulib_tsl_handshake (&s) != 0)
    goto done;
  while (off < n)
    {
      size_t got;
      if (mulib_tsl_write (&s, (const unsigned char *)request + off, n - off,
                           &got)
          || !got)
        goto done;
      off += got;
    }
  for (;;)
    {
      size_t got;
      int r = mulib_tsl_read (&s, buffer, sizeof (buffer), &got);
      if (r == MULIB_TSL_EOF)
        {
          ok = 1;
          break;
        }
      if (r || !got || !receive (receiver, buffer, got))
        break;
    }
done:
  mulib_tsl_close (&s);
  return ok;
}
mulib_webclient_result
mulib_https_request (const char *url, const char *method,
                     const mulib_http_header_t *headers, size_t count,
                     const void *body, size_t length,
                     const mulib_webclient_options_t *options,
                     mulib_https_connect_fn connect, void *user,
                     mulib_http_message_t *response)
{
  mulib_https_context_ ctx;
  mulib_uri_t clear;
  char *routing;
  mulib_webclient_result r;
  if (!url || !connect || !mulib_uri_parse (url, strlen (url), &ctx.endpoint)
      || ctx.endpoint.scheme.length != 5 || !ctx.endpoint.has_authority
      || !ctx.endpoint.host.length || ctx.endpoint.has_userinfo
      || ctx.endpoint.has_fragment)
    return MULIB_WEBCLIENT_INVALID;
  for (unsigned i = 0; i < 5; ++i)
    {
      unsigned char c = (unsigned char)ctx.endpoint.scheme.data[i];
      if (c >= 'A' && c <= 'Z')
        c = (unsigned char)(c + 32);
      if (c != (unsigned char)"https"[i])
        return MULIB_WEBCLIENT_INVALID;
    }
  if (!ctx.endpoint.has_port)
    {
      ctx.endpoint.has_port = 1;
      ctx.endpoint.port.data = "443";
      ctx.endpoint.port.length = 3;
    }
  clear = ctx.endpoint;
  clear.scheme.data = "http";
  clear.scheme.length = 4;
  routing = mulib_uri_compose (&clear);
  if (!routing)
    return MULIB_WEBCLIENT_INVALID;
  ctx.connect = connect;
  ctx.user = user;
  r = mulib_webclient_request (routing, method, headers, count, body, length,
                               options, mulib_https_transport_, &ctx,
                               response);
  free (routing);
  return r;
}
#endif
