/* MuLIB: Checked ULEB128 and ZigZag codecs for portable signed/unsigned record storage.
 * C11 single-header; define MULIB_IMPL_ENCODING once. */
#ifndef MULIB_ENCODING_H
#define MULIB_ENCODING_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  size_t mulib_encoding_uvarint (uint64_t value, unsigned char out[10]);
  /* Returns number of bytes used or 0 for truncated/overflow/invalid. */
  size_t mulib_encoding_read_uvarint (const unsigned char *data, size_t length,
                                      uint64_t *value);
  uint64_t mulib_encoding_zigzag (int64_t value);
  int64_t mulib_encoding_unzigzag (uint64_t value);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ENCODING) && !defined(MULIB_ENCODING_IMPLEMENTED)
#define MULIB_ENCODING_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  size_t
  mulib_encoding_uvarint (uint64_t x, unsigned char out[10])
  {
    if (!out)
      return 0;
    size_t n = 0;
    do
      {
        unsigned char c = (unsigned char)(x & 127);
        x >>= 7;
        out[n++] = (unsigned char)(c | (x ? 128 : 0));
      }
    while (x);
    return n;
  }
  size_t
  mulib_encoding_read_uvarint (const unsigned char *p, size_t n, uint64_t *out)
  {
    if (!p || !out)
      return 0;
    uint64_t v = 0;
    for (size_t i = 0; i < n && i < 10; i++)
      {
        unsigned c = p[i];
        if (i == 9 && c > 1)
          return 0;
        v |= (uint64_t)(c & 127) << (7 * i);
        if (!(c & 128))
          {
            *out = v;
            return i + 1;
          }
      }
    return 0;
  }
  uint64_t
  mulib_encoding_zigzag (int64_t x)
  {
    uint64_t v = (uint64_t)x;
    return (v << 1) ^ (UINT64_C (0) - ((v >> 63) & 1));
  }
  int64_t
  mulib_encoding_unzigzag (uint64_t x)
  {
    uint64_t u = (x >> 1) ^ (UINT64_C (0) - (x & 1));
    if (u <= INT64_MAX)
      return (int64_t)u;
    return -1 - (int64_t)(UINT64_MAX - u);
  }
#ifdef __cplusplus
}
#endif
#endif
