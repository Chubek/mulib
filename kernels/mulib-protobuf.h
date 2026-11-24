/** @file mulib-protobuf.h
 * @brief Schema-free Protobuf wire fields (varint/fixed64/bytes/fixed32).
 * Define MULIB_IMPL_PROTOBUF, VARINT and BUFFER. Groups are unsupported.
 * Read: 1 field, 0 truncated, -1 invalid. Failure unchanged. Write: 1/0,
 * failure-atomic append. No generated messages or implicit signed conversion.
 */
#ifndef MULIB_PROTOBUF_H
#define MULIB_PROTOBUF_H
#include "mulib-buffer.h"
#include "mulib-varint.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_protobuf_field
  {
    uint32_t number;
    unsigned wire;
    uint64_t value;
    const unsigned char *data;
    size_t length;
  } mulib_protobuf_field_t;
  int mulib_protobuf_read (const void *data, size_t length,
                           mulib_protobuf_field_t *out, size_t *consumed);
  int mulib_protobuf_write (mulib_buffer_t *out,
                            const mulib_protobuf_field_t *field);
  uint64_t mulib_protobuf_zigzag (int64_t value);
  int64_t mulib_protobuf_unzigzag (uint64_t value);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PROTOBUF) && !defined(MULIB_PROTOBUF_IMPLEMENTED)
#define MULIB_PROTOBUF_IMPLEMENTED
static int
mulib_protobuf_var_ (const unsigned char *s, size_t n, uint64_t *v,
                     size_t *used)
{
  if (mulib_varint_decode (s, n, v, used))
    return 1;
  for (size_t i = 0; i < n && i < 10; ++i)
    if (!(s[i] & 128))
      return -1;
  return n >= 10 ? -1 : 0;
}
int
mulib_protobuf_read (const void *p, size_t n, mulib_protobuf_field_t *out,
                     size_t *used)
{
  const unsigned char *s = (const unsigned char *)p;
  mulib_protobuf_field_t f = { 0 };
  uint64_t tag, len;
  size_t at, k;
  int rc;
  if ((!p && n) || !out || !used)
    return -1;
  rc = mulib_protobuf_var_ (s, n, &tag, &at);
  if (rc != 1)
    return rc;
  if (!(tag >> 3) || (tag >> 3) > 0x1fffffff)
    return -1;
  f.number = (uint32_t)(tag >> 3);
  f.wire = (unsigned)(tag & 7);
  if (f.wire == 0)
    {
      rc = mulib_protobuf_var_ (s + at, n - at, &f.value, &k);
      if (rc != 1)
        return rc;
      at += k;
    }
  else if (f.wire == 2)
    {
      rc = mulib_protobuf_var_ (s + at, n - at, &len, &k);
      if (rc != 1)
        return rc;
      at += k;
      if (len > n - at)
        return 0;
      f.data = s + at;
      f.length = (size_t)len;
      at += f.length;
    }
  else if (f.wire == 1 || f.wire == 5)
    {
      k = f.wire == 1 ? 8 : 4;
      if (n - at < k)
        return 0;
      for (size_t i = 0; i < k; ++i)
        f.value |= (uint64_t)s[at + i] << (8 * i);
      at += k;
    }
  else
    return -1;
  *out = f;
  *used = at;
  return 1;
}
int
mulib_protobuf_write (mulib_buffer_t *out, const mulib_protobuf_field_t *f)
{
  unsigned char h[20];
  size_t n, k;
  mulib_buffer_t b = { 0 };
  int ok;
  if (!out || !f || !f->number || f->number > 0x1fffffff
      || (f->wire != 0 && f->wire != 1 && f->wire != 2 && f->wire != 5)
      || (f->wire == 2 && !f->data && f->length)
      || (f->wire == 5 && f->value > UINT32_MAX))
    return 0;
  n = mulib_varint_encode (((uint64_t)f->number << 3) | f->wire, h);
  if (!f->wire)
    n += mulib_varint_encode (f->value, h + n);
  else if (f->wire == 2)
    n += mulib_varint_encode (f->length, h + n);
  else
    {
      k = f->wire == 1 ? 8 : 4;
      for (size_t i = 0; i < k; ++i)
        h[n++] = (unsigned char)(f->value >> (8 * i));
    }
  ok = mulib_buffer_append (&b, h, n)
       && (f->wire != 2 || mulib_buffer_append (&b, f->data, f->length))
       && mulib_buffer_append (out, b.data, b.length);
  mulib_buffer_free (&b);
  return ok;
}
uint64_t
mulib_protobuf_zigzag (int64_t v)
{
  return v < 0 ? ((uint64_t)(-(v + 1)) << 1) | 1 : (uint64_t)v << 1;
}
int64_t
mulib_protobuf_unzigzag (uint64_t v)
{
  return v & 1 ? -1 - (int64_t)(v >> 1) : (int64_t)(v >> 1);
}
#endif
