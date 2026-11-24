/* MuLIB: Pack/unpack unsigned fixed-width values 1..32 bits with explicit byte bounds.
 * C11 single-header; define MULIB_IMPL_BITPACK once. */
#ifndef MULIB_BITPACK_H
#define MULIB_BITPACK_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  size_t mulib_bitpack_bytes (size_t count, unsigned width);
  int mulib_bitpack_encode (const uint32_t *values, size_t count, unsigned width,
                            unsigned char *out, size_t out_bytes);
  int mulib_bitpack_decode (const unsigned char *input, size_t input_bytes,
                            size_t count, unsigned width, uint32_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BITPACK) && !defined(MULIB_BITPACK_IMPLEMENTED)
#define MULIB_BITPACK_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  size_t
  mulib_bitpack_bytes (size_t count, unsigned w)
  {
    if (!w || w > 32 || count > (SIZE_MAX - 7) / w)
      return SIZE_MAX;
    return (count * w + 7) / 8;
  }
  int
  mulib_bitpack_encode (const uint32_t *v, size_t n, unsigned w, unsigned char *out,
                        size_t bytes)
  {
    size_t need = mulib_bitpack_bytes (n, w);
    if (need == SIZE_MAX || need > bytes || (!v && n) || (!out && need))
      return 0;
    uint32_t mask = w == 32 ? UINT32_MAX : ((UINT32_C (1) << w) - 1);
    for (size_t i = 0; i < n; i++)
      if ((v[i] & mask) != v[i])
        return 0;
    if (need)
      memset (out, 0, need);
    for (size_t i = 0; i < n; i++)
      for (unsigned b = 0; b < w; b++)
        {
          size_t bit = i * w + b;
          out[bit / 8] |= (unsigned char)(((v[i] >> b) & 1U) << (bit % 8));
        }
    return 1;
  }
  int
  mulib_bitpack_decode (const unsigned char *in, size_t bytes, size_t n, unsigned w,
                        uint32_t *out)
  {
    size_t need = mulib_bitpack_bytes (n, w);
    if (need == SIZE_MAX || need > bytes || (!out && n) || (!in && need))
      return 0;
    for (size_t i = 0; i < n; i++)
      {
        uint32_t v = 0;
        for (unsigned b = 0; b < w; b++)
          {
            size_t bit = i * w + b;
            v |= (uint32_t)((in[bit / 8] >> (bit % 8)) & 1U) << b;
          }
        out[i] = v;
      }
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
