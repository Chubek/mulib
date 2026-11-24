/** @file mulib-cbor.h
 * @brief RFC 8949 JSON-profile CBOR: null, bool, finite numbers, UTF-8 text,
 * arrays and text-keyed maps. Reject tags, byte strings, indefinite lengths,
 * duplicate keys, half floats and integers outside exact JSON double range.
 * Define MULIB_IMPL_CBOR, JSON, BUFFER and JSON dependencies.
 * Encode appends atomically, decode owns a value and requires exact input.
 * Maximum nesting 64; decode input bounded to 16 MiB. */
#ifndef MULIB_CBOR_H
#define MULIB_CBOR_H
#include "mulib-buffer.h"
#include "mulib-json.h"
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_cbor_encode (const mulib_json_t *value, mulib_buffer_t *out);
  mulib_json_t *mulib_cbor_decode (const void *data, size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CBOR) && !defined(MULIB_CBOR_IMPLEMENTED)
#define MULIB_CBOR_IMPLEMENTED
#include <float.h>
static int
mulib_cbor_head_ (mulib_buffer_t *b, unsigned major, uint64_t v)
{
  unsigned char h[9];
  size_t n = v < 24            ? 0
             : v <= 255        ? 1
             : v <= 65535      ? 2
             : v <= UINT32_MAX ? 4
                               : 8;
  h[0] = (unsigned char)((major << 5)
                         | (n == 0   ? v
                            : n == 1 ? 24
                            : n == 2 ? 25
                            : n == 4 ? 26
                                     : 27));
  for (size_t i = 0; i < n; ++i)
    h[i + 1] = (unsigned char)(v >> (8 * (n - 1 - i)));
  return mulib_buffer_append (b, h, n + 1);
}
static int
mulib_cbor_emit_ (const mulib_json_t *v, mulib_buffer_t *b, unsigned depth)
{
  size_t n;
  const char *s;
  uint64_t bits;
  double d;
  if (depth > 64)
    return 0;
  switch (mulib_json_typeof (v))
    {
    case MULIB_JSON_NULL:
      {
        unsigned char x = 0xf6;
        return mulib_buffer_append (b, &x, 1);
      }
    case MULIB_JSON_BOOL:
      {
        unsigned char x = (unsigned char)(0xf4 + mulib_json_boolean (v));
        return mulib_buffer_append (b, &x, 1);
      }
    case MULIB_JSON_NUMBER:
      if (sizeof (double) != 8 || DBL_MANT_DIG != 53)
        return 0;
      d = mulib_json_numeric (v);
      memcpy (&bits, &d, 8);
      {
        unsigned char h[9];
        h[0] = 0xfb;
        for (unsigned i = 0; i < 8; ++i)
          h[i + 1] = (unsigned char)(bits >> (56 - 8 * i));
        return mulib_buffer_append (b, h, 9);
      }
    case MULIB_JSON_STRING:
      s = mulib_json_text (v, &n);
      return mulib_cbor_head_ (b, 3, n) && mulib_buffer_append (b, s, n);
    case MULIB_JSON_ARRAY:
    case MULIB_JSON_OBJECT:
      n = mulib_json_size (v);
      if (!mulib_cbor_head_ (
              b, mulib_json_typeof (v) == MULIB_JSON_ARRAY ? 4 : 5, n))
        return 0;
      for (size_t i = 0; i < n; ++i)
        {
          if (mulib_json_typeof (v) == MULIB_JSON_OBJECT)
            {
              size_t len;
              s = mulib_json_key (v, i, &len);
              if (!mulib_cbor_head_ (b, 3, len)
                  || !mulib_buffer_append (b, s, len))
                return 0;
            }
          /* Numeric values always use float64, including small integers. */
          if (!mulib_cbor_emit_ (mulib_json_at (v, i), b, depth + 1))
            return 0;
        }
      return 1;
    }
  return 0;
}
int
mulib_cbor_encode (const mulib_json_t *v, mulib_buffer_t *out)
{
  mulib_buffer_t b = { 0 };
  int ok = out && mulib_cbor_emit_ (v, &b, 0)
           && mulib_buffer_append (out, b.data, b.length);
  mulib_buffer_free (&b);
  return ok;
}
static int
mulib_cbor_take_ (const unsigned char *s, size_t n, size_t *at, unsigned *major,
                  unsigned *ai, uint64_t *v)
{
  size_t k;
  unsigned h;
  if (*at >= n)
    return 0;
  h = s[(*at)++];
  *major = h >> 5;
  *ai = h & 31;
  *v = *ai;
  if (*ai < 24)
    return 1;
  if (*ai > 27)
    return 0;
  k = (size_t)1 << (*ai - 24);
  if (k > n - *at)
    return 0;
  *v = 0;
  for (size_t i = 0; i < k; ++i)
    *v = (*v << 8) | s[(*at)++];
  return 1;
}
static mulib_json_t *
mulib_cbor_parse_ (const unsigned char *s, size_t n, size_t *at, unsigned depth)
{
  unsigned major, ai;
  uint64_t x;
  mulib_json_t *v = NULL, *child = NULL, *key = NULL;
  if (depth > 64 || !mulib_cbor_take_ (s, n, at, &major, &ai, &x))
    return NULL;
  if (major <= 1)
    {
      if (x
          > (major ? UINT64_C (9007199254740990) : UINT64_C (9007199254740991)))
        return NULL;
      return mulib_json_number (major ? -1.0 - (double)x : (double)x);
    }
  if (major == 3)
    {
      if (x > n - *at)
        return NULL;
      v = mulib_json_string_n ((const char *)s + *at, (size_t)x);
      *at += (size_t)x;
      return v;
    }
  if (major == 7)
    {
      if (ai == 20 || ai == 21)
        return mulib_json_bool (ai == 21);
      if (ai == 22)
        return mulib_json_new (MULIB_JSON_NULL);
      if (ai == 27 && sizeof (double) == 8 && DBL_MANT_DIG == 53)
        {
          double d;
          memcpy (&d, &x, 8);
          return mulib_json_number (d);
        }
      if (ai == 26 && sizeof (float) == 4 && FLT_MANT_DIG == 24)
        {
          float f;
          uint32_t bits = (uint32_t)x;
          memcpy (&f, &bits, 4);
          return mulib_json_number (f);
        }
      return NULL;
    }
  if ((major != 4 && major != 5) || x > (n - *at) / (major == 5 ? 2U : 1U))
    return NULL;
  v = mulib_json_new (major == 4 ? MULIB_JSON_ARRAY : MULIB_JSON_OBJECT);
  if (!v)
    return NULL;
  for (uint64_t i = 0; i < x; ++i)
    {
      if (major == 5)
        {
          size_t len;
          const char *text;
          key = mulib_cbor_parse_ (s, n, at, depth + 1);
          text = mulib_json_text (key, &len);
          if (!text || mulib_json_get_n (v, text, len))
            goto fail;
          child = mulib_cbor_parse_ (s, n, at, depth + 1);
          if (!child || !mulib_json_set_n (v, text, len, child))
            goto fail;
          mulib_json_destroy (key);
          key = NULL;
        }
      else
        {
          child = mulib_cbor_parse_ (s, n, at, depth + 1);
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
mulib_cbor_decode (const void *p, size_t n)
{
  size_t at = 0;
  mulib_json_t *v;
  if (!p || !n || n > 16U * 1024U * 1024U)
    return NULL;
  v = mulib_cbor_parse_ ((const unsigned char *)p, n, &at, 0);
  if (at != n)
    {
      mulib_json_destroy (v);
      return NULL;
    }
  return v;
}
#endif
