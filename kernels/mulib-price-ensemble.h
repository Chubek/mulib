/** @file mulib-price-ensemble.h
 * @brief Error-weighted forecast blending with intermodel dispersion.
 * C11 single-header module. Define MULIB_IMPL_PRICE_ENSEMBLE in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_PRICE_ENSEMBLE_H
#define MULIB_PRICE_ENSEMBLE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-price-ensemble.md for this operation's contract.
   */
  int mulib_price_ensemble_blend (const double *forecasts, const double *maes,
                                  size_t n, double *mean, double *stddev);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_ENSEMBLE_H */
#ifdef MULIB_IMPL_PRICE_ENSEMBLE
#ifndef MULIB_IMPL_PRICE_ENSEMBLE_ONCE
#define MULIB_IMPL_PRICE_ENSEMBLE_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_price_ensemble_blend (const double *f, const double *err, size_t n,
                            double *mean, double *sd)
{
  if (!f || !err || !mean || !sd || mean == sd || !n)
    return 0;
  long double w = 0, x = 0;
  for (size_t i = 0; i < n; i++)
    {
      if (!isfinite (f[i]) || f[i] < 0 || !isfinite (err[i]) || err[i] < 0)
        return 0;
      long double z = 1 / (err[i] + 1e-9L);
      w += z;
      x += z * f[i];
    }
  if (w <= 0)
    return 0;
  long double m = x / w, v = 0;
  for (size_t i = 0; i < n; i++)
    {
      long double z = 1 / (err[i] + 1e-9L), d = f[i] - m;
      v += z * d * d;
    }
  double result = (double)m, spread = (double)sqrtl (v / w);
  if (!isfinite (result) || !isfinite (spread))
    return 0;
  *mean = result;
  *sd = spread;
  return 1;
}
#endif
#endif
