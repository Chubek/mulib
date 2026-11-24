/** @file mulib-http1.h
 * @brief Explicit HTTP/1.1 entry point reusing strict shared framing.
 * Define MULIB_IMPL_HTTP1 and HTTP dependencies. */
#ifndef MULIB_HTTP1_H
#define MULIB_HTTP1_H
#include "mulib-http.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** Parse one HTTP/1.1 message with the exact HTTP ownership, framing and
   * limit contract. On failure out is unchanged and consumed (if given) is 0.
   * Response context flags are mutually exclusive and invalid for requests. */
  mulib_http_result mulib_http1_parse (const void *data, size_t length,
                                       int response,
                                       const mulib_http_options_t *options,
                                       mulib_http_message_t *out,
                                        size_t *consumed);
  /** Serialize validated HTTP/1.1; identical to mulib_http_serialize.
   * On OK transfers malloc-owned bytes with an extra NUL outside length.
   * On failure both output arguments remain unchanged. */
  mulib_http_result mulib_http1_serialize (const mulib_http_message_t *,
                                          const mulib_http_options_t *,
                                          unsigned char **bytes, size_t *length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HTTP1) && !defined(MULIB_HTTP1_IMPLEMENTED)
#define MULIB_HTTP1_IMPLEMENTED
mulib_http_result
mulib_http1_parse (const void *p, size_t n, int response,
                   const mulib_http_options_t *opts, mulib_http_message_t *out,
                   size_t *used)
{
  return mulib_http_parse (p, n, response, opts, out, used);
}
mulib_http_result
mulib_http1_serialize (const mulib_http_message_t *m,
                       const mulib_http_options_t *opts,
                       unsigned char **bytes, size_t *length)
{
  return mulib_http_serialize (m, opts, bytes, length);
}
#endif
