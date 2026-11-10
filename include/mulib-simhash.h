/* MuLIB: 64-bit weighted SimHash fingerprints and Hamming distance. C11 single-header;
 * define MULIB_IMPL_SIMHASH once. */
#ifndef MULIB_SIMHASH_H
#define MULIB_SIMHASH_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  uint64_t mulib_simhash64 (const uint64_t *token_hash, const double *weight, size_t n);
  unsigned mulib_simhash_distance (uint64_t a, uint64_t b);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SIMHASH) && !defined(MULIB_SIMHASH_IMPLEMENTED)
#define MULIB_SIMHASH_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif
  uint64_t
  mulib_simhash64 (const uint64_t *h, const double *w, size_t n)
  {
    if ((!h || !w) && n)
      return 0;
    double scale = 0;
    for (size_t i = 0; i < n; i++)
      {
        if (!isfinite (w[i]) || w[i] < 0)
          return 0;
        if (w[i] > scale)
          scale = w[i];
      }
    if (!scale)
      return 0;
    long double acc[64] = { 0 };
    for (size_t i = 0; i < n; i++)
      {
        long double weight = (long double)w[i] / scale;
        for (unsigned b = 0; b < 64; b++)
          acc[b] += (h[i] & (UINT64_C (1) << b)) ? weight : -weight;
      }
    uint64_t s = 0;
    for (unsigned b = 0; b < 64; b++)
      if (acc[b] > 0)
        s |= UINT64_C (1) << b;
    return s;
  }
  unsigned
  mulib_simhash_distance (uint64_t a, uint64_t b)
  {
    uint64_t x = a ^ b;
    unsigned count = 0;
    while (x)
      {
        x &= x - 1;
        count++;
      }
    return count;
  }
#ifdef __cplusplus
}
#endif
#endif
