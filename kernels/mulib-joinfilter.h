/* MuLIB: Bounded Bloom join filter with no false negatives for inserted keys. C11
 * single-header; define MULIB_IMPL_JOINFILTER once. */
#ifndef MULIB_JOINFILTER_H
#define MULIB_JOINFILTER_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    unsigned char *bits;
    size_t bit_count;
    uint64_t seed;
  } mulib_joinfilter_t;
  int mulib_joinfilter_init (mulib_joinfilter_t *f, size_t bit_count, uint64_t seed);
  void mulib_joinfilter_add (mulib_joinfilter_t *f, const void *key, size_t length);
  int mulib_joinfilter_maybe (const mulib_joinfilter_t *f, const void *key,
                              size_t length);
  void mulib_joinfilter_free (mulib_joinfilter_t *f);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_JOINFILTER) && !defined(MULIB_JOINFILTER_IMPLEMENTED)
#define MULIB_JOINFILTER_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  static uint64_t
  mulib_joinfilter_hash_ (const void *key, size_t n, uint64_t seed)
  {
    uint64_t h = UINT64_C (14695981039346656037) ^ seed;
    const unsigned char *p = (const unsigned char *)key;
    for (size_t i = 0; i < n; i++)
      {
        h ^= p[i];
        h *= UINT64_C (1099511628211);
      }
    return h;
  }
  int
  mulib_joinfilter_init (mulib_joinfilter_t *f, size_t n, uint64_t seed)
  {
    if (!f || f->bits || !n || n > SIZE_MAX - 7)
      return 0;
    f->bits = (unsigned char *)calloc ((n + 7) / 8, 1);
    if (!f->bits)
      return 0;
    f->bit_count = n;
    f->seed = seed;
    return 1;
  }
  void
  mulib_joinfilter_add (mulib_joinfilter_t *f, const void *key, size_t n)
  {
    if (!f || !f->bits || (!key && n))
      return;
    uint64_t a
        = mulib_joinfilter_hash_ (key, n, f->seed),
        b
        = mulib_joinfilter_hash_ (key, n, f->seed + UINT64_C (0x9e3779b97f4a7c15)) | 1;
    for (unsigned i = 0; i < 4; i++)
      {
        size_t k = (size_t)((a + b * i) % f->bit_count);
        f->bits[k / 8] |= (unsigned char)(1u << (k % 8));
      }
  }
  int
  mulib_joinfilter_maybe (const mulib_joinfilter_t *f, const void *key, size_t n)
  {
    if (!f || !f->bits || (!key && n))
      return 0;
    uint64_t a
        = mulib_joinfilter_hash_ (key, n, f->seed),
        b
        = mulib_joinfilter_hash_ (key, n, f->seed + UINT64_C (0x9e3779b97f4a7c15)) | 1;
    for (unsigned i = 0; i < 4; i++)
      {
        size_t k = (size_t)((a + b * i) % f->bit_count);
        if (!(f->bits[k / 8] & (1u << (k % 8))))
          return 0;
      }
    return 1;
  }
  void
  mulib_joinfilter_free (mulib_joinfilter_t *f)
  {
    if (!f)
      return;
    free (f->bits);
    memset (f, 0, sizeof (*f));
  }
#ifdef __cplusplus
}
#endif
#endif
