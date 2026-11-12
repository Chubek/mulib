/** @file mulib-price-causal.h
 * @brief Difference-in-differences average treatment effect diagnostic.
 * C11 single-header module. Define MULIB_IMPL_PRICE_CAUSAL in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_PRICE_CAUSAL_H
#define MULIB_PRICE_CAUSAL_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_price_causal_result
  {
    double effect, standard_error;
    size_t treated_n, control_n;
  } mulib_price_causal_result_t;
  /* Each array contains matched before/after observations for its group.
   * Requires >=2 per group. */
  int mulib_price_causal_did (const double *treated_before,
                              const double *treated_after, size_t nt,
                              const double *control_before,
                              const double *control_after, size_t nc,
                              mulib_price_causal_result_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_CAUSAL_H */
#ifdef MULIB_IMPL_PRICE_CAUSAL
#ifndef MULIB_IMPL_PRICE_CAUSAL_ONCE
#define MULIB_IMPL_PRICE_CAUSAL_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_price_causal_did (const double *ta, const double *tb, size_t nt,
                        const double *ca, const double *cb, size_t nc,
                        mulib_price_causal_result_t *o)
{
  if (!ta || !tb || !ca || !cb || !o || nt < 2 || nc < 2)
    return 0;
  long double mt = 0, mc = 0, vt = 0, vc = 0;
  for (size_t i = 0; i < nt; i++)
    {
      if (!isfinite (ta[i]) || !isfinite (tb[i]))
        return 0;
      mt += (long double)tb[i] - ta[i];
    }
  for (size_t i = 0; i < nc; i++)
    {
      if (!isfinite (ca[i]) || !isfinite (cb[i]))
        return 0;
      mc += (long double)cb[i] - ca[i];
    }
  mt /= nt;
  mc /= nc;
  for (size_t i = 0; i < nt; i++)
    {
      long double d = (long double)tb[i] - ta[i] - mt;
      vt += d * d;
    }
  for (size_t i = 0; i < nc; i++)
    {
      long double d = (long double)cb[i] - ca[i] - mc;
      vc += d * d;
    }
  mulib_price_causal_result_t z
      = { (double)(mt - mc),
          (double)sqrtl (vt / ((long double)(nt - 1) * nt)
                         + vc / ((long double)(nc - 1) * nc)),
          nt, nc };
  if (!isfinite (z.effect) || !isfinite (z.standard_error))
    return 0;
  *o = z;
  return 1;
}
#endif
#endif
