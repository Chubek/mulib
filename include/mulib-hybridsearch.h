/* MuLIB: Reciprocal rank fusion of two ranked ID arrays. C11 single-header; define
 * MULIB_IMPL_HYBRIDSEARCH once. */
#ifndef MULIB_HYBRIDSEARCH_H
#define MULIB_HYBRIDSEARCH_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t id;
    double score;
  } mulib_hybridsearch_hit_t;
  /* Arrays are ordered best first; only the first occurrence per list scores.
   * Return unique IDs sorted by descending score, then ascending ID.
   * SIZE_MAX means invalid/capacity error; output is unchanged on failure. */
  size_t mulib_hybridsearch_rrf (const uint64_t *lexical, size_t lex_count,
                                 const uint64_t *vector, size_t vec_count,
                                 double offset, mulib_hybridsearch_hit_t *out,
                                 size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HYBRIDSEARCH) && !defined(MULIB_HYBRIDSEARCH_IMPLEMENTED)
#define MULIB_HYBRIDSEARCH_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_hybridsearch_compare_ (const void *aa, const void *bb)
  {
    const mulib_hybridsearch_hit_t *a = (const mulib_hybridsearch_hit_t *)aa,
                                   *b = (const mulib_hybridsearch_hit_t *)bb;
    if (a->score > b->score)
      return -1;
    if (a->score < b->score)
      return 1;
    return a->id < b->id ? -1 : a->id > b->id;
  }
  size_t
  mulib_hybridsearch_rrf (const uint64_t *a, size_t an, const uint64_t *b, size_t bn,
                          double k, mulib_hybridsearch_hit_t *out, size_t cap)
  {
    if ((!a && an) || (!b && bn) || !isfinite (k) || k < 0 || (!out && cap)
        || an > SIZE_MAX - bn)
      return SIZE_MAX;
    size_t required = 0;
    for (size_t i = 0; i < an; i++)
      {
        int fresh = 1;
        for (size_t j = 0; j < i; j++)
          if (a[i] == a[j])
            {
              fresh = 0;
              break;
            }
        required += (size_t)fresh;
      }
    for (size_t i = 0; i < bn; i++)
      {
        int fresh = 1;
        for (size_t j = 0; j < an; j++)
          if (b[i] == a[j])
            {
              fresh = 0;
              break;
            }
        for (size_t j = 0; j < i && fresh; j++)
          if (b[i] == b[j])
            fresh = 0;
        required += (size_t)fresh;
      }
    if (required > cap)
      return SIZE_MAX;
    size_t n = 0;
    for (int side = 0; side < 2; side++)
      {
        const uint64_t *arr = side ? b : a;
        size_t len = side ? bn : an;
        for (size_t i = 0; i < len; i++)
          {
            size_t prior = 0;
            while (prior < i && arr[prior] != arr[i])
              prior++;
            if (prior < i)
              continue;
            size_t j = 0;
            while (j < n && out[j].id != arr[i])
              j++;
            if (j == n)
              {
                out[n++] = (mulib_hybridsearch_hit_t){ arr[i], 0 };
              }
            out[j].score += 1.0 / (k + 1.0 + (double)i);
          }
      }
    if (n > 1)
      qsort (out, n, sizeof (*out), mulib_hybridsearch_compare_);
    return n;
  }
#ifdef __cplusplus
}
#endif
#endif
