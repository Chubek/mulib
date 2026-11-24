/* MuLIB: Deterministic Count-Min Sketch for estimated frequencies. C11 single-header;
 * define MULIB_IMPL_COUNTMIN once. */
#ifndef MULIB_COUNTMIN_H
#define MULIB_COUNTMIN_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t *cells;
    size_t width, depth;
  } mulib_countmin_t;
  int mulib_countmin_init (mulib_countmin_t *s, size_t width, size_t depth);
  int mulib_countmin_add (mulib_countmin_t *s, const void *key, size_t bytes,
                          uint64_t count);
  uint64_t mulib_countmin_estimate (const mulib_countmin_t *s, const void *key,
                                    size_t bytes);
  void mulib_countmin_free (mulib_countmin_t *s);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_COUNTMIN) && !defined(MULIB_COUNTMIN_IMPLEMENTED)
#define MULIB_COUNTMIN_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  static uint64_t
  mulib_countmin_hash_ (const void *key, size_t n, uint64_t seed)
  {
    const unsigned char *p = (const unsigned char *)key;
    uint64_t h = UINT64_C (14695981039346656037) ^ seed;
    for (size_t i = 0; i < n; i++)
      {
        h ^= p[i];
        h *= UINT64_C (1099511628211);
      }
    return h;
  }
  int
  mulib_countmin_init (mulib_countmin_t *s, size_t w, size_t d)
  {
    if (!s || s->cells || !w || !d || w > SIZE_MAX / d
        || w * d > SIZE_MAX / sizeof (uint64_t))
      return 0;
    uint64_t *p = (uint64_t *)calloc (w * d, sizeof (uint64_t));
    if (!p)
      return 0;
    s->cells = p;
    s->width = w;
    s->depth = d;
    return 1;
  }
  int
  mulib_countmin_add (mulib_countmin_t *s, const void *key, size_t n, uint64_t count)
  {
    if (!s || !s->cells || (!key && n))
      return 0;
    for (size_t i = 0; i < s->depth; i++)
      {
        size_t j = (size_t)(mulib_countmin_hash_ (
                                key, n, UINT64_C (0x9e3779b97f4a7c15) * (i + 1))
                            % s->width);
        uint64_t *p = &s->cells[i * s->width + j];
        *p = UINT64_MAX - *p < count ? UINT64_MAX : *p + count;
      }
    return 1;
  }
  uint64_t
  mulib_countmin_estimate (const mulib_countmin_t *s, const void *key, size_t n)
  {
    if (!s || !s->cells || (!key && n))
      return 0;
    uint64_t min = UINT64_MAX;
    for (size_t i = 0; i < s->depth; i++)
      {
        size_t j = (size_t)(mulib_countmin_hash_ (
                                key, n, UINT64_C (0x9e3779b97f4a7c15) * (i + 1))
                            % s->width);
        uint64_t x = s->cells[i * s->width + j];
        if (x < min)
          min = x;
      }
    return min;
  }
  void
  mulib_countmin_free (mulib_countmin_t *s)
  {
    if (!s)
      return;
    free (s->cells);
    memset (s, 0, sizeof (*s));
  }
#ifdef __cplusplus
}
#endif
#endif
