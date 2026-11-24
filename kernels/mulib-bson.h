/** @file mulib-bson.h
 * @brief BSON JSON profile: documents, arrays, strings, doubles, booleans,
 * null and exact-range int32/int64 decoding. No binary/ObjectId/date types.
 * Define MULIB_IMPL_BSON, JSON, BUFFER and JSON dependencies.
 * Root must be object; duplicate keys/NUL keys rejected. Atomic encode append;
 * decode exact owned value, max depth 64 and input 16 MiB. */
#ifndef MULIB_BSON_H
#define MULIB_BSON_H
#include "mulib-buffer.h"
#include "mulib-json.h"
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_bson_encode (const mulib_json_t *document, mulib_buffer_t *out);
  mulib_json_t *mulib_bson_decode (const void *data, size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BSON) && !defined(MULIB_BSON_IMPLEMENTED)
#define MULIB_BSON_IMPLEMENTED
#include <float.h>
static int
mulib_bson_uint_ (mulib_buffer_t *b, uint64_t x, unsigned width)
{
  unsigned char h[8];
  for (unsigned i = 0; i < width; ++i)
    h[i] = (unsigned char)(x >> (8 * i));
  return mulib_buffer_append (b, h, width);
}
static uint64_t
mulib_bson_load_ (const unsigned char *s, unsigned width)
{
  uint64_t v = 0;
  for (unsigned i = 0; i < width; ++i)
    v |= (uint64_t)s[i] << (8 * i);
  return v;
}
static int
mulib_bson_document_ (const mulib_json_t *v, mulib_buffer_t *b, unsigned depth)
{
  size_t start = b->length, n = mulib_json_size (v);
  unsigned char zero = 0;
  int array = mulib_json_typeof (v) == MULIB_JSON_ARRAY;
  if (depth > 64 || (!array && mulib_json_typeof (v) != MULIB_JSON_OBJECT)
      || !mulib_bson_uint_ (b, 0, 4))
    return 0;
  for (size_t i = 0; i < n; ++i)
    {
      const mulib_json_t *child = mulib_json_at (v, i);
      size_t len;
      const char *key;
      char index[32];
      unsigned char type;
      mulib_json_type t = mulib_json_typeof (child);
      if (array)
        {
          int k = snprintf (index, sizeof (index), "%zu", i);
          if (k < 0 || (size_t)k >= sizeof (index))
            return 0;
          key = index;
          len = (size_t)k;
        }
      else
        {
          key = mulib_json_key (v, i, &len);
          if (memchr (key, 0, len))
            return 0;
        }
      type = (unsigned char)(t == MULIB_JSON_NULL     ? 10
                             : t == MULIB_JSON_BOOL   ? 8
                             : t == MULIB_JSON_NUMBER ? 1
                             : t == MULIB_JSON_STRING ? 2
                             : t == MULIB_JSON_OBJECT ? 3
                                                      : 4);
      if (!mulib_buffer_append (b, &type, 1)
          || !mulib_buffer_append (b, key, len)
          || !mulib_buffer_append (b, &zero, 1))
        return 0;
      if (type == 1)
        {
          double d = mulib_json_numeric (child);
          uint64_t bits;
          if (sizeof (double) != 8 || DBL_MANT_DIG != 53)
            return 0;
          memcpy (&bits, &d, 8);
          if (!mulib_bson_uint_ (b, bits, 8))
            return 0;
        }
      else if (type == 2)
        {
          const char *s = mulib_json_text (child, &len);
          if (len >= INT32_MAX || !mulib_bson_uint_ (b, len + 1, 4)
              || !mulib_buffer_append (b, s, len)
              || !mulib_buffer_append (b, &zero, 1))
            return 0;
        }
      else if (type == 3 || type == 4)
        {
          if (!mulib_bson_document_ (child, b, depth + 1))
            return 0;
        }
      else if (type == 8)
        {
          unsigned char x = (unsigned char)mulib_json_boolean (child);
          if (!mulib_buffer_append (b, &x, 1))
            return 0;
        }
      if (b->length - start >= INT32_MAX)
        return 0;
    }
  if (!mulib_buffer_append (b, &zero, 1) || b->length - start > INT32_MAX)
    return 0;
  for (unsigned i = 0; i < 4; ++i)
    b->data[start + i] = (unsigned char)((b->length - start) >> (8 * i));
  return 1;
}
int
mulib_bson_encode (const mulib_json_t *v, mulib_buffer_t *out)
{
  mulib_buffer_t b = { 0 };
  int ok = out && mulib_json_typeof (v) == MULIB_JSON_OBJECT
           && mulib_bson_document_ (v, &b, 0)
           && mulib_buffer_append (out, b.data, b.length);
  mulib_buffer_free (&b);
  return ok;
}
static mulib_json_t *
mulib_bson_parse_ (const unsigned char *s, size_t n, int array, unsigned depth,
                   size_t *used)
{
  size_t size, at = 4, count = 0;
  mulib_json_t *v, *child = NULL;
  if (depth > 64 || n < 5 || (size = (size_t)mulib_bson_load_ (s, 4)) < 5
      || size > n || size > INT32_MAX || s[size - 1])
    return NULL;
  v = mulib_json_new (array ? MULIB_JSON_ARRAY : MULIB_JSON_OBJECT);
  if (!v)
    return NULL;
  while (at < size - 1)
    {
      unsigned type = s[at++];
      const unsigned char *end
          = (const unsigned char *)memchr (s + at, 0, size - 1 - at);
      const char *key = (const char *)s + at;
      size_t len, k;
      uint64_t x;
      if (!end)
        goto fail;
      len = (size_t)(end - (s + at));
      at += len + 1;
      if (array)
        {
          char index[32];
          int r = snprintf (index, sizeof (index), "%zu", count);
          if (r < 0 || (size_t)r != len || memcmp (key, index, len))
            goto fail;
        }
      else if (mulib_json_get_n (v, key, len))
        goto fail;
      k = type == 1 || type == 18 ? 8 : type == 16 ? 4 : type == 8 ? 1 : 0;
      if (k > size - 1 - at)
        goto fail;
      if (type == 1)
        {
          double d;
          if (sizeof (double) != 8 || DBL_MANT_DIG != 53)
            goto fail;
          x = mulib_bson_load_ (s + at, 8);
          memcpy (&d, &x, 8);
          child = mulib_json_number (d);
          at += 8;
        }
      else if (type == 16 || type == 18)
        {
          double d;
          x = mulib_bson_load_ (s + at, (unsigned)k);
          at += k;
          if (x & (UINT64_C (1) << (k * 8 - 1)))
            {
              uint64_t magnitude = k == 8 ? ~x : UINT32_MAX ^ x;
              if (magnitude > UINT64_C (9007199254740990))
                goto fail;
              d = -1.0 - (double)magnitude;
            }
          else
            {
              if (x > UINT64_C (9007199254740991))
                goto fail;
              d = (double)x;
            }
          child = mulib_json_number (d);
        }
      else if (type == 8)
        {
          if (s[at] > 1)
            goto fail;
          child = mulib_json_bool (s[at++]);
        }
      else if (type == 10)
        child = mulib_json_new (MULIB_JSON_NULL);
      else if (type == 2)
        {
          if (size - 1 - at < 4)
            goto fail;
          k = (size_t)mulib_bson_load_ (s + at, 4);
          at += 4;
          if (!k || k > INT32_MAX || k > size - 1 - at || s[at + k - 1])
            goto fail;
          child = mulib_json_string_n ((const char *)s + at, k - 1);
          at += k;
        }
      else if (type == 3 || type == 4)
        {
          child = mulib_bson_parse_ (s + at, size - 1 - at, type == 4,
                                     depth + 1, &k);
          if (!child)
            goto fail;
          at += k;
        }
      else
        goto fail;
      if (!child
          || !(array ? mulib_json_append (v, child)
                     : mulib_json_set_n (v, key, len, child)))
        goto fail;
      child = NULL;
      ++count;
    }
  if (at != size - 1)
    goto fail;
  *used = size;
  return v;
fail:
  mulib_json_destroy (child);
  mulib_json_destroy (v);
  return NULL;
}
mulib_json_t *
mulib_bson_decode (const void *p, size_t n)
{
  size_t used = 0;
  mulib_json_t *v;
  if (!p || n > 16U * 1024U * 1024U)
    return NULL;
  v = mulib_bson_parse_ ((const unsigned char *)p, n, 0, 0, &used);
  if (used != n)
    {
      mulib_json_destroy (v);
      return NULL;
    }
  return v;
}
#endif
