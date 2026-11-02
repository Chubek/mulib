/** @file mulib-compression.h
 * @brief Explicit identity/RLE buffer transforms, not gzip/HTTP negotiation.
 * Define MULIB_IMPL_COMPRESSION, BUFFER and RLE. Append is failure-atomic.
 * Decoder requires a hard output limit to bound decompression expansion. */
#ifndef MULIB_COMPRESSION_H
#define MULIB_COMPRESSION_H
#include "mulib-buffer.h"
#include "mulib-rle.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum mulib_compression
  {
    MULIB_COMPRESSION_IDENTITY,
    MULIB_COMPRESSION_RLE
  } mulib_compression;
  int mulib_compression_encode (mulib_compression codec, const void *data,
                                size_t length, mulib_buffer_t *out);
  int mulib_compression_decode (mulib_compression codec, const void *data,
                                size_t length, size_t limit,
                                mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_COMPRESSION) && !defined(MULIB_COMPRESSION_IMPLEMENTED)
#define MULIB_COMPRESSION_IMPLEMENTED
int
mulib_compression_encode (mulib_compression c, const void *p, size_t n,
                          mulib_buffer_t *out)
{
  unsigned char *tmp;
  size_t used;
  int ok;
  if (!out || (!p && n))
    return 0;
  if (c == MULIB_COMPRESSION_IDENTITY)
    return mulib_buffer_append (out, p, n);
  if (c != MULIB_COMPRESSION_RLE || n > SIZE_MAX / 2)
    return 0;
  tmp = (unsigned char *)malloc (n ? n * 2 : 1);
  if (!tmp)
    return 0;
  used = mulib_rle_encode ((const unsigned char *)p, n, tmp, n * 2);
  ok = (!n || used) && mulib_buffer_append (out, tmp, used);
  free (tmp);
  return ok;
}
int
mulib_compression_decode (mulib_compression c, const void *p, size_t n,
                          size_t limit, mulib_buffer_t *out)
{
  const unsigned char *s = (const unsigned char *)p;
  size_t total = 0, used;
  unsigned char *tmp;
  int ok;
  if (!out || (!p && n))
    return 0;
  if (c == MULIB_COMPRESSION_IDENTITY)
    return n <= limit && mulib_buffer_append (out, p, n);
  if (c != MULIB_COMPRESSION_RLE || n % 2)
    return 0;
  for (size_t i = 0; i < n; i += 2)
    {
      if (!s[i] || s[i] > limit - total)
        return 0;
      total += s[i];
    }
  tmp = (unsigned char *)malloc (total ? total : 1);
  if (!tmp)
    return 0;
  ok = mulib_rle_decode (s, n, tmp, total, &used)
       && mulib_buffer_append (out, tmp, used);
  free (tmp);
  return ok;
}
#endif
