/** @file mulib-http2.h
 * @brief HTTP/2 frame envelopes with payload and SETTINGS validation.
 * Define MULIB_IMPL_HTTP2 and BUFFER. No HPACK, flow control or connection
 * state machine. Payload borrows input. Unknown types/flags are preserved.
 * Parse 1 OK, 0 MORE, -1 invalid, -2 limit; failure unchanged. */
#ifndef MULIB_HTTP2_H
#define MULIB_HTTP2_H
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_http2_frame
  {
    unsigned char type, flags;
    uint32_t stream;
    const unsigned char *data;
    size_t length;
  } mulib_http2_frame_t;
   /** Read one complete frame, retaining unknown types/flags. max_payload is
    * an explicit limit (zero allows only empty payloads); no SETTINGS default
    * is assumed. The reserved stream bit is ignored on input. On MORE/error,
    * both out and consumed remain unchanged. Payload borrows input. */
   int mulib_http2_frame_read (const void *data, size_t length,
                              size_t max_payload, mulib_http2_frame_t *out,
                              size_t *consumed);
   /** Append one validated frame transactionally: 1 success, 0 invalid/OOM.
    * out must be initialized; frame payload may alias its existing bytes.
    * Stream identifiers must fit 31 bits. No peer/connection state is tracked. */
   int mulib_http2_frame_write (mulib_buffer_t *out,
                               const mulib_http2_frame_t *frame);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HTTP2) && !defined(MULIB_HTTP2_IMPLEMENTED)
#define MULIB_HTTP2_IMPLEMENTED
static uint32_t
mulib_http2_u32_ (const unsigned char *p)
{
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16
         | (uint32_t)p[2] << 8 | p[3];
}
static int
mulib_http2_settings_ (const unsigned char *p, size_t n)
{
  for (size_t i = 0; i < n; i += 6) {
    unsigned id = (unsigned)p[i] << 8 | p[i + 1];
    uint32_t value = mulib_http2_u32_ (p + i + 2);
    if ((id == 2 || id == 8 || id == 9) && value > 1) return 0;
    if (id == 4 && value > UINT32_C (0x7fffffff)) return 0;
    if (id == 5 && (value < 16384 || value > 0xffffff)) return 0;
  }
  /* Repeated identifiers are legal in HTTP/2 and are applied in wire order.
   * Unrecognized identifiers are ignored by peers, not rejected by the codec. */
  return 1;
}
static int
mulib_http2_valid_ (const mulib_http2_frame_t *f)
{
  size_t skip = 0, prefix = 0;
  if ((!f->data && f->length) || f->length > 0xffffff || f->stream > 0x7fffffff)
    return 0;
  switch (f->type)
    {
    case 0:
    case 1:
    case 5:
      if (!f->stream)
        return 0;
      if (f->flags & 8)
        {
          if (!f->length || f->data[0] >= f->length)
            return 0;
          skip = (size_t)f->data[0] + 1;
          prefix = 1;
        }
      if (f->type == 1 && (f->flags & 32))
        skip += 5;
      if (f->type == 5)
        skip += 4;
      if (skip > f->length) return 0;
      if (f->type == 1 && (f->flags & 32)
          && (mulib_http2_u32_ (f->data + prefix) & UINT32_C (0x7fffffff)) == f->stream)
        return 0;
      if (f->type == 5
          && !(mulib_http2_u32_ (f->data + prefix) & UINT32_C (0x7fffffff)))
        return 0;
      return 1;
    case 2:
      return f->stream && f->length == 5
             && (mulib_http2_u32_ (f->data) & UINT32_C (0x7fffffff)) != f->stream;
    case 3:
      return f->stream && f->length == 4;
    case 4:
      return !f->stream && ((f->flags & 1) ? !f->length
               : f->length % 6 == 0 && mulib_http2_settings_ (f->data, f->length));
    case 6:
      return !f->stream && f->length == 8;
    case 7:
      return !f->stream && f->length >= 8;
    case 8:
      return f->length == 4
             && ((f->data[0] & 127) || f->data[1] || f->data[2] || f->data[3]);
    case 9:
      return f->stream != 0;
    default:
      return 1;
    }
}
int
mulib_http2_frame_read (const void *p, size_t n, size_t max,
                        mulib_http2_frame_t *out, size_t *used)
{
  const unsigned char *s = (const unsigned char *)p;
  mulib_http2_frame_t f;
  if ((!p && n) || !out || !used)
    return -1;
  if (n < 9)
    return 0;
  f.length = ((size_t)s[0] << 16) | ((size_t)s[1] << 8) | s[2];
  if (f.length > max)
    return -2;
  if (f.length > n - 9)
    return 0;
  f.type = s[3];
  f.flags = s[4];
  f.stream = ((uint32_t)(s[5] & 127) << 24) | ((uint32_t)s[6] << 16)
             | ((uint32_t)s[7] << 8) | s[8];
  f.data = s + 9;
  if (!mulib_http2_valid_ (&f))
    return -1;
  *out = f;
  *used = f.length + 9;
  return 1;
}
int
mulib_http2_frame_write (mulib_buffer_t *out, const mulib_http2_frame_t *f)
{
  unsigned char h[9];
  mulib_buffer_t b = { 0 };
  int ok;
  if (!out || !f || !mulib_http2_valid_ (f))
    return 0;
  h[0] = (unsigned char)(f->length >> 16);
  h[1] = (unsigned char)(f->length >> 8);
  h[2] = (unsigned char)f->length;
  h[3] = f->type;
  h[4] = f->flags;
  for (unsigned i = 0; i < 4; ++i)
    h[5 + i] = (unsigned char)(f->stream >> (24 - 8 * i));
  ok = mulib_buffer_append (&b, h, 9)
       && mulib_buffer_append (&b, f->data, f->length)
       && mulib_buffer_append (out, b.data, b.length);
  mulib_buffer_free (&b);
  return ok;
}
#endif
