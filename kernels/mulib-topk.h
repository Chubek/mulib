/* MuLIB: Stable deterministic top-k selection of scored identifiers. C11 single-header;
 * define MULIB_IMPL_TOPK once. */
#ifndef MULIB_TOPK_H
#define MULIB_TOPK_H
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
  } mulib_topk_entry_t;
  /* Order: descending score, ascending id. Returns min(k,n), SIZE_MAX invalid. */
  size_t mulib_topk_select (const mulib_topk_entry_t *in, size_t n,
                            mulib_topk_entry_t *out, size_t k);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TOPK) && !defined(MULIB_TOPK_IMPLEMENTED)
#define MULIB_TOPK_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_topk_better_ (mulib_topk_entry_t a, mulib_topk_entry_t b)
  {
    return a.score > b.score || (a.score == b.score && a.id < b.id);
  }
  size_t
  mulib_topk_select (const mulib_topk_entry_t *v, size_t n, mulib_topk_entry_t *out,
                     size_t k)
  {
    if ((!v && n) || (!out && k) || (v == out && n))
      return SIZE_MAX;
    size_t used = 0;
    for (size_t i = 0; i < n; i++)
      {
        if (!isfinite (v[i].score))
          return SIZE_MAX;
      }
    for (size_t i = 0; i < n; i++)
      {
        size_t pos = 0;
        while (pos < used && !mulib_topk_better_ (v[i], out[pos]))
          pos++;
        if (pos >= k)
          continue;
        if (used < k)
          used++;
        memmove (out + pos + 1, out + pos, (used - pos - 1) * sizeof (*out));
        out[pos] = v[i];
      }
    return used;
  }
#ifdef __cplusplus
}
#endif
#endif
