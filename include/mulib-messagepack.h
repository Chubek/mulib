/** @file mulib-messagepack.h
 * @brief MessagePack JSON profile: null/bool/finite numbers/text/arrays/maps.
 * Reject binary/ext values, non-string/duplicate map keys and integers outside
 * exact double range. Float32/64 accepted; numbers encoded as float64.
 * Define MULIB_IMPL_MESSAGEPACK, JSON, BUFFER and JSON dependencies.
 * Encode atomic append; decode exact owned value, depth 64, input <=16 MiB. */
#ifndef MULIB_MESSAGEPACK_H
#define MULIB_MESSAGEPACK_H
#include "mulib-buffer.h"
#include "mulib-json.h"
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_messagepack_encode (const mulib_json_t *value, mulib_buffer_t *out);
  mulib_json_t *mulib_messagepack_decode (const void *data, size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MESSAGEPACK) && !defined(MULIB_MESSAGEPACK_IMPLEMENTED)
#define MULIB_MESSAGEPACK_IMPLEMENTED
#include <float.h>
static int
mulib_messagepack_uint_ (mulib_buffer_t *b, unsigned tag, uint64_t v,
                         unsigned width)
{
  unsigned char h[9];
  h[0] = (unsigned char)tag;
  for (unsigned i = 0; i < width; ++i)
    h[i + 1] = (unsigned char)(v >> (8 * (width - 1 - i)));
  return mulib_buffer_append (b, h, width + 1);
}
static int
mulib_messagepack_text_ (mulib_buffer_t *b, const char *s, size_t n)
{
  if (n > UINT32_MAX)
    return 0;
  return (n < 32 ? mulib_messagepack_uint_ (b, (unsigned)(0xa0 + n), 0, 0)
                 : mulib_messagepack_uint_ (b, 0xdb, n, 4))
         && mulib_buffer_append (b, s, n);
}
static int
mulib_messagepack_emit_ (const mulib_json_t *v, mulib_buffer_t *b,
                         unsigned depth)
{
  size_t n;
  const char *s;
  double d;
  uint64_t bits;
  if (depth > 64)
    return 0;
  switch (mulib_json_typeof (v))
    {
    case MULIB_JSON_NULL:
      return mulib_messagepack_uint_ (b, 0xc0, 0, 0);
    case MULIB_JSON_BOOL:
      return mulib_messagepack_uint_ (b, 0xc2 + mulib_json_boolean (v), 0, 0);
    case MULIB_JSON_NUMBER:
      if (sizeof (double) != 8 || DBL_MANT_DIG != 53)
        return 0;
      d = mulib_json_numeric (v);
      memcpy (&bits, &d, 8);
      return mulib_messagepack_uint_ (b, 0xcb, bits, 8);
    case MULIB_JSON_STRING:
      s = mulib_json_text (v, &n);
      return mulib_messagepack_text_ (b, s, n);
    case MULIB_JSON_ARRAY:
    case MULIB_JSON_OBJECT:
      n = mulib_json_size (v);
      if (n > UINT32_MAX
          || !mulib_messagepack_uint_ (
              b, mulib_json_typeof (v) == MULIB_JSON_ARRAY ? 0xdd : 0xdf, n, 4))
        return 0;
      for (size_t i = 0; i < n; ++i)
        {
          if (mulib_json_typeof (v) == MULIB_JSON_OBJECT)
            {
              size_t k;
              s = mulib_json_key (v, i, &k);
              if (!mulib_messagepack_text_ (b, s, k))
                return 0;
            }
          if (!mulib_messagepack_emit_ (mulib_json_at (v, i), b, depth + 1))
            return 0;
        }
      return 1;
    }
  return 0;
}
int
mulib_messagepack_encode (const mulib_json_t *v, mulib_buffer_t *out)
{
  mulib_buffer_t b = { 0 };
  int ok = out && mulib_messagepack_emit_ (v, &b, 0)
           && mulib_buffer_append (out, b.data, b.length);
  mulib_buffer_free (&b);
  return ok;
}
static int
mulib_messagepack_take_ (const unsigned char *s, size_t n, size_t *at,
                         unsigned width, uint64_t *v)
{
  if (width > n - *at)
    return 0;
  *v = 0;
  for (unsigned i = 0; i < width; ++i)
    *v = (*v << 8) | s[(*at)++];
  return 1;
}
static mulib_json_t *
mulib_messagepack_parse_ (const unsigned char *s, size_t n, size_t *at,
                          unsigned depth)
{
  unsigned tag, kind = 0, width = 0;
  uint64_t x = 0;
  mulib_json_t *v = NULL, *key = NULL, *child = NULL;
  if (depth > 64 || *at >= n)
    return NULL;
  tag = s[(*at)++];
  if (tag <= 127)
    return mulib_json_number (tag);
  if (tag >= 224)
    return mulib_json_number ((int)tag - 256);
  if (tag == 0xc0)
    return mulib_json_new (MULIB_JSON_NULL);
  if (tag == 0xc2 || tag == 0xc3)
    return mulib_json_bool (tag == 0xc3);
  if ((tag & 0xe0) == 0xa0)
    {
      kind = 1;
      x = tag & 31;
    }
  else if ((tag & 0xf0) == 0x90)
    {
      kind = 2;
      x = tag & 15;
    }
  else if ((tag & 0xf0) == 0x80)
    {
      kind = 3;
      x = tag & 15;
    }
  else if (tag >= 0xcc && tag <= 0xd3)
    {
      width = 1U << ((tag - 0xcc) & 3);
      if (!mulib_messagepack_take_ (s, n, at, width, &x))
        return NULL;
      if (tag >= 0xd0 && (x & (UINT64_C (1) << (width * 8 - 1))))
        {
          uint64_t magnitude
              = width == 8 ? ~x : ((UINT64_C (1) << (width * 8)) - 1) ^ x;
          if (magnitude > UINT64_C (9007199254740990))
            return NULL;
          return mulib_json_number (-1.0 - (double)magnitude);
        }
      if (x > UINT64_C (9007199254740991))
        return NULL;
      return mulib_json_number ((double)x);
    }
  else if (tag == 0xca || tag == 0xcb)
    {
      if (!mulib_messagepack_take_ (s, n, at, tag == 0xca ? 4 : 8, &x))
        return NULL;
      if (tag == 0xcb && sizeof (double) == 8 && DBL_MANT_DIG == 53)
        {
          double d;
          memcpy (&d, &x, 8);
          return mulib_json_number (d);
        }
      if (tag == 0xca && sizeof (float) == 4 && FLT_MANT_DIG == 24)
        {
          float f;
          uint32_t bits = (uint32_t)x;
          memcpy (&f, &bits, 4);
          return mulib_json_number (f);
        }
      return NULL;
    }
  else
    {
      if (tag >= 0xd9 && tag <= 0xdb)
        {
          kind = 1;
          width = 1U << (tag - 0xd9);
        }
      else if (tag == 0xdc || tag == 0xdd)
        {
          kind = 2;
          width = tag == 0xdc ? 2 : 4;
        }
      else if (tag == 0xde || tag == 0xdf)
        {
          kind = 3;
          width = tag == 0xde ? 2 : 4;
        }
      else
        return NULL;
      if (!mulib_messagepack_take_ (s, n, at, width, &x))
        return NULL;
    }
  if (kind == 1)
    {
      if (x > n - *at)
        return NULL;
      v = mulib_json_string_n ((const char *)s + *at, (size_t)x);
      *at += (size_t)x;
      return v;
    }
  if (x > (n - *at) / (kind == 3 ? 2U : 1U))
    return NULL;
  v = mulib_json_new (kind == 2 ? MULIB_JSON_ARRAY : MULIB_JSON_OBJECT);
  if (!v)
    return NULL;
  for (uint64_t i = 0; i < x; ++i)
    {
      if (kind == 3)
        {
          size_t len;
          const char *text;
          key = mulib_messagepack_parse_ (s, n, at, depth + 1);
          text = mulib_json_text (key, &len);
          if (!text || mulib_json_get_n (v, text, len))
            goto fail;
          child = mulib_messagepack_parse_ (s, n, at, depth + 1);
          if (!child || !mulib_json_set_n (v, text, len, child))
            goto fail;
          mulib_json_destroy (key);
          key = NULL;
        }
      else
        {
          child = mulib_messagepack_parse_ (s, n, at, depth + 1);
          if (!child || !mulib_json_append (v, child))
            goto fail;
        }
      child = NULL;
    }
  return v;
fail:
  mulib_json_destroy (key);
  mulib_json_destroy (child);
  mulib_json_destroy (v);
  return NULL;
}
mulib_json_t *
mulib_messagepack_decode (const void *p, size_t n)
{
  size_t at = 0;
  mulib_json_t *v;
  if (!p || !n || n > 16U * 1024U * 1024U)
    return NULL;
  v = mulib_messagepack_parse_ ((const unsigned char *)p, n, &at, 0);
  if (at != n)
    {
      mulib_json_destroy (v);
      return NULL;
    }
  return v;
}
#endif
