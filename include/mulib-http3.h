/** @file mulib-http3.h
 * @brief Bounded HTTP/3 type/length frame envelope codec, RFC 9114.
 * Define MULIB_IMPL_HTTP3, QUIC and BUFFER. Unknown frame types are preserved.
 * No QPACK, stream-role validation, SETTINGS semantics or QUIC transport.
 * Parse returns 1 OK, 0 MORE, -1 invalid, -2 limit; failure unchanged. */
#ifndef MULIB_HTTP3_H
#define MULIB_HTTP3_H
#include "mulib-quic.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_http3_frame
  {
    uint64_t type;
    const unsigned char *data;
    size_t length;
  } mulib_http3_frame_t;
  int mulib_http3_frame_read (const void *data, size_t length,
                              size_t max_payload, mulib_http3_frame_t *out,
                              size_t *consumed);
  int mulib_http3_frame_write (mulib_buffer_t *out,
                               const mulib_http3_frame_t *frame);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HTTP3) && !defined(MULIB_HTTP3_IMPLEMENTED)
#define MULIB_HTTP3_IMPLEMENTED
int
mulib_http3_frame_read (const void *p, size_t n, size_t max,
                        mulib_http3_frame_t *out, size_t *used)
{
  const unsigned char *s = (const unsigned char *)p;
  mulib_http3_frame_t f;
  uint64_t len;
  size_t at, k;
  int rc;
  if (!out || !used || (!p && n))
    return -1;
  rc = mulib_quic_varint_read (p, n, &f.type, &at);
  if (rc != 1)
    return rc;
  rc = mulib_quic_varint_read (s + at, n - at, &len, &k);
  if (rc != 1)
    return rc;
  at += k;
  if (len > max)
    return -2;
  if (len > n - at)
    return 0;
  f.data = s + at;
  f.length = (size_t)len;
  *out = f;
  *used = at + f.length;
  return 1;
}
int
mulib_http3_frame_write (mulib_buffer_t *out, const mulib_http3_frame_t *f)
{
  mulib_buffer_t b = { 0 };
  int ok;
  if (!out || !f || (!f->data && f->length))
    return 0;
  ok = mulib_quic_varint_write (&b, f->type)
       && mulib_quic_varint_write (&b, f->length)
       && mulib_buffer_append (&b, f->data, f->length)
       && mulib_buffer_append (out, b.data, b.length);
  mulib_buffer_free (&b);
  return ok;
}
#endif
