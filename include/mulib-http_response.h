/** @file mulib-http_response.h
 * @brief Owned response construction over the existing webframework helper.
 * Define MULIB_IMPL_HTTP_RESPONSE and WEBFRAMEWORK/HTTP dependencies. */
#ifndef MULIB_HTTP_RESPONSE_H
#define MULIB_HTTP_RESPONSE_H
#include "mulib-webframework.h"
#ifdef __cplusplus
extern "C"
{
#endif
  mulib_http_result mulib_http_response_create (mulib_http_message_t *out,
                                                unsigned status,
                                                const char *content_type,
                                                const void *body,
                                                size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HTTP_RESPONSE)                                          \
    && !defined(MULIB_HTTP_RESPONSE_IMPLEMENTED)
#define MULIB_HTTP_RESPONSE_IMPLEMENTED
mulib_http_result
mulib_http_response_create (mulib_http_message_t *out, unsigned s,
                            const char *type, const void *p, size_t n)
{
  return mulib_webframework_reply (out, s, type, p, n);
}
#endif
