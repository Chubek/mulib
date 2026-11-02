/** @file mulib-quic.h
 * @brief RFC 9000 variable integers and length-bearing STREAM frame codecs.
 * Define MULIB_IMPL_QUIC and BUFFER. No packet protection, TLS, congestion,
 * ACK processing or transport; never send these as unauthenticated packets.
 * Parse: 1 OK, 0 truncated, -1 invalid. Views borrow input; failure unchanged.
 */
#ifndef MULIB_QUIC_H
#define MULIB_QUIC_H
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C"
{
#endif
#define MULIB_QUIC_MAX_INTEGER UINT64_C (0x3fffffffffffffff)
  int mulib_quic_varint_read (const void *data, size_t length, uint64_t *value,
                              size_t *consumed);
  int mulib_quic_varint_write (mulib_buffer_t *out, uint64_t value);
  typedef struct mulib_quic_stream
  {
    uint64_t stream_id, offset;
    const unsigned char *data;
    size_t length;
    int fin;
  } mulib_quic_stream_t;
  int mulib_quic_stream_read (const void *data, size_t length,
                              mulib_quic_stream_t *out, size_t *consumed);
  int mulib_quic_stream_write (mulib_buffer_t *out,
                               const mulib_quic_stream_t *stream);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_QUIC) && !defined(MULIB_QUIC_IMPLEMENTED)
#define MULIB_QUIC_IMPLEMENTED
int
mulib_quic_varint_read (const void *p, size_t n, uint64_t *out, size_t *used)
{
  const unsigned char *s = (const unsigned char *)p;
  uint64_t v;
  size_t k;
  if ((!p && n) || !out || !used)
    return -1;
  if (!n)
    return 0;
  k = (size_t)1 << (s[0] >> 6);
  if (n < k)
    return 0;
  v = s[0] & 63;
  for (size_t i = 1; i < k; ++i)
    v = (v << 8) | s[i];
  *out = v;
  *used = k;
  return 1;
}
int
mulib_quic_varint_write (mulib_buffer_t *out, uint64_t v)
{
  unsigned char bytes[8];
  size_t n = v < 64 ? 1 : v < 16384 ? 2 : v < UINT64_C (1073741824) ? 4 : 8;
  if (v > MULIB_QUIC_MAX_INTEGER)
    return 0;
  for (size_t i = n; i; --i)
    {
      bytes[i - 1] = (unsigned char)v;
      v >>= 8;
    }
  bytes[0] |= (unsigned char)(n == 1 ? 0 : n == 2 ? 64 : n == 4 ? 128 : 192);
  return mulib_buffer_append (out, bytes, n);
}
int
mulib_quic_stream_read (const void *p, size_t n, mulib_quic_stream_t *out,
                        size_t *used)
{
  const unsigned char *s = (const unsigned char *)p;
  mulib_quic_stream_t f;
  uint64_t type, len;
  size_t at, k;
  int rc;
  if ((!p && n) || !out || !used)
    return -1;
  rc = mulib_quic_varint_read (p, n, &type, &at);
  if (rc != 1)
    return rc;
  if (type < 10 || type > 15 || !(type & 2))
    return -1;
  rc = mulib_quic_varint_read (s + at, n - at, &f.stream_id, &k);
  if (rc != 1)
    return rc;
  at += k;
  f.offset = 0;
  if (type & 4)
    {
      rc = mulib_quic_varint_read (s + at, n - at, &f.offset, &k);
      if (rc != 1)
        return rc;
      at += k;
    }
  rc = mulib_quic_varint_read (s + at, n - at, &len, &k);
  if (rc != 1)
    return rc;
  at += k;
  if (len > MULIB_QUIC_MAX_INTEGER - f.offset)
    return -1;
  if (len > n - at)
    return 0;
  f.data = s + at;
  f.length = (size_t)len;
  f.fin = (int)(type & 1);
  *out = f;
  *used = at + f.length;
  return 1;
}
int
mulib_quic_stream_write (mulib_buffer_t *out, const mulib_quic_stream_t *f)
{
  mulib_buffer_t b = { 0 };
  int ok;
  if (!out || !f || (!f->data && f->length)
      || f->offset > MULIB_QUIC_MAX_INTEGER
      || f->length > MULIB_QUIC_MAX_INTEGER - f->offset
      || (f->fin != 0 && f->fin != 1))
    return 0;
  ok = mulib_quic_varint_write (&b, (uint64_t)(14 | f->fin))
       && mulib_quic_varint_write (&b, f->stream_id)
       && mulib_quic_varint_write (&b, f->offset)
       && mulib_quic_varint_write (&b, f->length)
       && mulib_buffer_append (&b, f->data, f->length)
       && mulib_buffer_append (out, b.data, b.length);
  mulib_buffer_free (&b);
  return ok;
}
#endif
