/** @file mulib-webextract-reconcile.h
 * @brief Evidence-aware consensus of candidate numeric price extractions.
 * C11 single-header module. Define MULIB_IMPL_WEBEXTRACT_RECONCILE in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_WEBEXTRACT_RECONCILE_H
#define MULIB_WEBEXTRACT_RECONCILE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_webextract_candidate
  {
    double price, confidence;
    unsigned source_priority;
    int has_evidence;
  } mulib_webextract_candidate_t;
  int
  /** @brief See manual/mulib-webextract-reconcile.md for this operation's
     contract. */
  mulib_webextract_reconcile (const mulib_webextract_candidate_t *candidates,
                              size_t n, double relative_tolerance,
                              double *price, size_t *selected);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_WEBEXTRACT_RECONCILE_H */
#ifdef MULIB_IMPL_WEBEXTRACT_RECONCILE
#ifndef MULIB_IMPL_WEBEXTRACT_RECONCILE_ONCE
#define MULIB_IMPL_WEBEXTRACT_RECONCILE_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_webextract_reconcile (const mulib_webextract_candidate_t *a, size_t n,
                            double tol, double *out, size_t *idx)
{
  if (!a || !n || n > 4096 || !out || !idx || !isfinite (tol) || tol < 0)
    return 0;
  for (size_t i = 0; i < n; i++)
    if (!isfinite (a[i].price) || a[i].price < 0 || !isfinite (a[i].confidence)
        || a[i].confidence < 0 || a[i].confidence > 1)
      return 0;
  long double best = -1;
  size_t bi = 0;
  for (size_t i = 0; i < n; i++)
    {
      long double weight = a[i].confidence * (a[i].has_evidence ? 1.25L : 0.5L)
                           + 0.01L * a[i].source_priority;
      for (size_t j = 0; j < n; j++)
        if (i != j
            && fabsl ((long double)a[i].price - a[j].price)
                   <= tol * fmaxl (1, a[i].price))
          weight += a[j].confidence * 0.2L;
      if (weight > best)
        {
          best = weight;
          bi = i;
        }
    }
  *out = a[bi].price;
  *idx = bi;
  return 1;
}
#endif
#endif
