/** @file mulib-http2.h
 * @brief HTTP/2 nine-byte frame envelopes with basic per-type validation.
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
  int mulib_http2_frame_read (const void *data, size_t length,
                              size_t max_payload, mulib_http2_frame_t *out,
                              size_t *consumed);
  int mulib_http2_frame_write (mulib_buffer_t *out,
                               const mulib_http2_frame_t *frame);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HTTP2) && !defined(MULIB_HTTP2_IMPLEMENTED)
#define MULIB_HTTP2_IMPLEMENTED
static int
mulib_http2_valid_ (const mulib_http2_frame_t *f)
{
  size_t skip = 0;
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
        }
      if (f->type == 1 && (f->flags & 32))
        skip += 5;
      if (f->type == 5)
        skip += 4;
      return skip <= f->length;
    case 2:
      return f->stream && f->length == 5;
    case 3:
      return f->stream && f->length == 4;
    case 4:
      return !f->stream && ((f->flags & 1) ? !f->length : f->length % 6 == 0);
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
