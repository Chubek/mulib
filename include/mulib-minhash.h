/* MuLIB: Seeded MinHash signatures for Jaccard similarity estimation. C11
 * single-header; define MULIB_IMPL_MINHASH once. */
#ifndef MULIB_MINHASH_H
#define MULIB_MINHASH_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  void mulib_minhash_init (uint64_t *signature, size_t count);
  void mulib_minhash_add (uint64_t *signature, size_t count, const void *key,
                          size_t bytes);
  double mulib_minhash_similarity (const uint64_t *a, const uint64_t *b, size_t count);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MINHASH) && !defined(MULIB_MINHASH_IMPLEMENTED)
#define MULIB_MINHASH_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  static uint64_t
  mulib_minhash_mix_ (uint64_t x)
  {
    x ^= x >> 30;
    x *= UINT64_C (0xbf58476d1ce4e5b9);
    x ^= x >> 27;
    x *= UINT64_C (0x94d049bb133111eb);
    return x ^ (x >> 31);
  }
  void
  mulib_minhash_init (uint64_t *s, size_t n)
  {
    if (s)
      for (size_t i = 0; i < n; i++)
        s[i] = UINT64_MAX;
  }
  void
  mulib_minhash_add (uint64_t *s, size_t n, const void *key, size_t bytes)
  {
    if (!s || (!key && bytes))
      return;
    const unsigned char *p = (const unsigned char *)key;
    uint64_t x = UINT64_C (14695981039346656037);
    for (size_t i = 0; i < bytes; i++)
      {
        x ^= p[i];
        x *= UINT64_C (1099511628211);
      }
    for (size_t i = 0; i < n; i++)
      {
        uint64_t h = mulib_minhash_mix_ (x ^ mulib_minhash_mix_ (i + 1));
        if (h < s[i])
          s[i] = h;
      }
  }
  double
  mulib_minhash_similarity (const uint64_t *a, const uint64_t *b, size_t n)
  {
    if (!a || !b || !n)
      return 0;
    size_t eq = 0;
    for (size_t i = 0; i < n; i++)
      eq += a[i] == b[i];
    return (double)eq / (double)n;
  }
#ifdef __cplusplus
}
#endif
#endif
