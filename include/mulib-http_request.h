/** @file mulib-http_request.h
 * @brief Owned request construction using the shared HTTP validator.
 * Define MULIB_IMPL_HTTP_REQUEST and all HTTP dependencies. */
#ifndef MULIB_HTTP_REQUEST_H
#define MULIB_HTTP_REQUEST_H
#include "mulib-http.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** Borrow inputs, create owned output on OK, unchanged otherwise. */
  mulib_http_result
  mulib_http_request_create (mulib_http_message_t *out, const char *method,
                             const char *target,
                             const mulib_http_header_t *headers, size_t count,
                             const void *body, size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HTTP_REQUEST) && !defined(MULIB_HTTP_REQUEST_IMPLEMENTED)
#define MULIB_HTTP_REQUEST_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
mulib_http_result
mulib_http_request_create (mulib_http_message_t *out, const char *method,
                           const char *target,
                           const mulib_http_header_t *headers, size_t count,
                           const void *body, size_t length)
{
  mulib_http_message_t m;
  unsigned char *wire = NULL;
  size_t n, used;
  mulib_http_result rc;
  if (!out)
    return MULIB_HTTP_INVALID;
  memset (&m, 0, sizeof (m));
  m.method = (char *)method;
  m.target = (char *)target;
  m.headers = (mulib_http_header_t *)headers;
  m.header_count = count;
  m.body = (unsigned char *)body;
  m.body_length = length;
  rc = mulib_http_serialize (&m, NULL, &wire, &n);
  if (rc == MULIB_HTTP_OK)
    rc = mulib_http_parse (wire, n, 0, NULL, out, &used);
  free (wire);
  return rc;
}
#endif
