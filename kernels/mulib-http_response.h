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
  /** Construct an independently owned response under default HTTP limits.
   * status must be 100..599; 1xx/204/205/304 must not carry content. Content-Type
   * is optional and validated against header injection. A nonempty body needs
   * a non-NULL pointer and is copied byte-for-byte. Failure leaves out unchanged;
   * successful output must be freed with message_free before reuse. The helper
   * supplies a reason phrase and canonical framing, not request HEAD/CONNECT
   * context; use the underlying HTTP message serializer for contextual replies. */
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
