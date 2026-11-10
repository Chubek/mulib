/* MuLIB: HyperLogLog cardinality sketch with 64-bit hashing and small-range correction.
 * C11 single-header; define MULIB_IMPL_HLL once. */
#ifndef MULIB_HLL_H
#define MULIB_HLL_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    unsigned char registers[1024];
  } mulib_hll_t;
  void mulib_hll_add (mulib_hll_t *h, const void *key, size_t length);
  double mulib_hll_estimate (const mulib_hll_t *h);
  void mulib_hll_merge (mulib_hll_t *dst, const mulib_hll_t *src);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HLL) && !defined(MULIB_HLL_IMPLEMENTED)
#define MULIB_HLL_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static uint64_t
  mulib_hll_hash_ (const void *key, size_t n)
  {
    const unsigned char *p = (const unsigned char *)key;
    uint64_t x = UINT64_C (14695981039346656037);
    for (size_t i = 0; i < n; i++)
      {
        x ^= p[i];
        x *= UINT64_C (1099511628211);
      }
    x ^= x >> 33;
    x *= UINT64_C (0xff51afd7ed558ccd);
    x ^= x >> 33;
    x *= UINT64_C (0xc4ceb9fe1a85ec53);
    return x ^ (x >> 33);
  }
  void
  mulib_hll_add (mulib_hll_t *h, const void *k, size_t n)
  {
    if (!h || (!k && n))
      return;
    uint64_t x = mulib_hll_hash_ (k, n);
    size_t i = (size_t)(x & 1023);
    uint64_t w = x >> 10;
    unsigned rank = 1;
    while (rank <= 54 && (w & (UINT64_C (1) << (54 - rank))) == 0)
      rank++;
    if (rank > h->registers[i])
      h->registers[i] = (unsigned char)rank;
  }
  double
  mulib_hll_estimate (const mulib_hll_t *h)
  {
    if (!h)
      return 0;
    double sum = 0;
    size_t zeros = 0;
    for (size_t i = 0; i < 1024; i++)
      {
        sum += ldexp (1.0, -(int)h->registers[i]);
        zeros += h->registers[i] == 0;
      }
    double v = 0.7213 / (1 + 1.079 / 1024.0) * 1024.0 * 1024.0 / sum;
    if (v <= 2560 && zeros)
      v = 1024.0 * log (1024.0 / (double)zeros);
    return v;
  }
  void
  mulib_hll_merge (mulib_hll_t *dst, const mulib_hll_t *src)
  {
    if (!dst || !src)
      return;
    for (size_t i = 0; i < 1024; i++)
      if (src->registers[i] > dst->registers[i])
        dst->registers[i] = src->registers[i];
  }
#ifdef __cplusplus
}
#endif
#endif
