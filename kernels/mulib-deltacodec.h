/* MuLIB: Monotone uint64 delta encoding and decoding with overflow checks. C11
 * single-header; define MULIB_IMPL_DELTACODEC once. */
#ifndef MULIB_DELTACODEC_H
#define MULIB_DELTACODEC_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_deltacodec_encode (const uint64_t *values, size_t count, uint64_t *deltas);
  int mulib_deltacodec_decode (const uint64_t *deltas, size_t count, uint64_t *values);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DELTACODEC) && !defined(MULIB_DELTACODEC_IMPLEMENTED)
#define MULIB_DELTACODEC_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_deltacodec_encode (const uint64_t *v, size_t n, uint64_t *out)
  {
    if ((!v || !out) && n)
      return 0;
    for (size_t i = 1; i < n; i++)
      if (v[i] < v[i - 1])
        return 0;
    uint64_t prev = 0;
    for (size_t i = 0; i < n; i++)
      {
        uint64_t x = v[i];
        out[i] = x - prev;
        prev = x;
      }
    return 1;
  }
  int
  mulib_deltacodec_decode (const uint64_t *v, size_t n, uint64_t *out)
  {
    if ((!v || !out) && n)
      return 0;
    uint64_t sum = 0;
    for (size_t i = 0; i < n; i++)
      {
        if (v[i] > UINT64_MAX - sum)
          return 0;
        sum += v[i];
      }
    sum = 0;
    for (size_t i = 0; i < n; i++)
      {
        sum += v[i];
        out[i] = sum;
      }
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
