/** @file mulib-web_conntection.h
 * @brief Bounded incremental HTTP/1 receive buffering (historical spelling).
 * Define MULIB_IMPL_WEB_CONNECTION, BUFFER and HTTP dependencies.
 * Zero initialize; configure limit and options before feed. No sockets/TLS. */
#ifndef MULIB_WEB_CONNTECTION_H
#define MULIB_WEB_CONNTECTION_H
#include "mulib-http1.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_web_connection
  {
    mulib_buffer_t pending;
    mulib_http_options_t options;
    size_t limit;
    int response, eof;
  } mulib_web_connection_t;
  mulib_http_result
  mulib_web_connection_feed (mulib_web_connection_t *connection,
                             const void *data, size_t length, int eof);
  /** Parse/remove exactly one message; pipelined remainder is retained. */
  mulib_http_result
  mulib_web_connection_next (mulib_web_connection_t *connection,
                             mulib_http_message_t *out);
  void mulib_web_connection_free (mulib_web_connection_t *connection);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WEB_CONNECTION)                                         \
    && !defined(MULIB_WEB_CONNECTION_IMPLEMENTED)
#define MULIB_WEB_CONNECTION_IMPLEMENTED
#include <string.h>
mulib_http_result
mulib_web_connection_feed (mulib_web_connection_t *c, const void *p, size_t n,
                           int eof)
{
  if (!c || c->eof || (!p && n))
    return MULIB_HTTP_INVALID;
  if (c->pending.length > c->limit || n > c->limit - c->pending.length)
    return MULIB_HTTP_LIMIT;
  if (!mulib_buffer_append (&c->pending, p, n))
    return MULIB_HTTP_NOMEM;
  c->eof = !!eof;
  return MULIB_HTTP_OK;
}
mulib_http_result
mulib_web_connection_next (mulib_web_connection_t *c, mulib_http_message_t *out)
{
  mulib_http_options_t options;
  mulib_http_result rc;
  size_t used;
  if (!c || !out)
    return MULIB_HTTP_INVALID;
  options = c->options;
  if (c->eof)
    options.flags |= MULIB_HTTP_EOF;
  rc = mulib_http1_parse (c->pending.data, c->pending.length, c->response,
                          &options, out, &used);
  if (rc == MULIB_HTTP_OK)
    (void)mulib_buffer_remove (&c->pending, 0, used);
  return rc;
}
void
mulib_web_connection_free (mulib_web_connection_t *c)
{
  if (c)
    {
      mulib_buffer_free (&c->pending);
      memset (c, 0, sizeof (*c));
    }
}
#endif
