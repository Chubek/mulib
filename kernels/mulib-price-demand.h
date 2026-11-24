/** @file mulib-price-demand.h
 * @brief Bounded linear demand estimation with closed-form ridge
 * regularization. C11 single-header module. Define MULIB_IMPL_PRICE_DEMAND in
 * exactly one C translation unit.
 *
 */
#ifndef MULIB_PRICE_DEMAND_H
#define MULIB_PRICE_DEMAND_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_price_demand_model
  {
    double intercept, slope, r_squared;
  } mulib_price_demand_model_t;
  /** @brief See manual/mulib-price-demand.md for this operation's contract. */
  int mulib_price_demand_fit (const double *prices, const double *units,
                              size_t n, double ridge,
                              mulib_price_demand_model_t *out);
  /** @brief See manual/mulib-price-demand.md for this operation's contract. */
  double mulib_price_demand_predict (const mulib_price_demand_model_t *model,
                                     double price);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_DEMAND_H */
#ifdef MULIB_IMPL_PRICE_DEMAND
#ifndef MULIB_IMPL_PRICE_DEMAND_ONCE
#define MULIB_IMPL_PRICE_DEMAND_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_price_demand_fit (const double *x, const double *y, size_t n,
                        double ridge, mulib_price_demand_model_t *o)
{
  if (!x || !y || !o || n < 2 || !isfinite (ridge) || ridge < 0)
    return 0;
  long double sx = 0, sy = 0, xx = 0, xy = 0, yy = 0;
  for (size_t i = 0; i < n; i++)
    {
      if (!isfinite (x[i]) || !isfinite (y[i]) || x[i] < 0 || y[i] < 0)
        return 0;
      sx += x[i];
      sy += y[i];
    }
  long double mx = sx / n, my = sy / n;
  for (size_t i = 0; i < n; i++)
    {
      long double a = x[i] - mx, b = y[i] - my;
      xx += a * a;
      xy += a * b;
      yy += b * b;
    }
  if (xx + ridge <= 0)
    return 0;
  long double b = xy / (xx + ridge), a = my - b * mx;
  long double err = 0;
  for (size_t i = 0; i < n; i++)
    {
      long double d = y[i] - (a + b * x[i]);
      err += d * d;
    }
  if (!isfinite (a) || !isfinite (b))
    return 0;
  mulib_price_demand_model_t z
      = { (double)a, (double)b, yy > 0 ? (double)(1 - err / yy) : 1 };
  if (!isfinite (z.intercept) || !isfinite (z.slope)
      || !isfinite (z.r_squared))
    return 0;
  *o = z;
  return 1;
}
double
mulib_price_demand_predict (const mulib_price_demand_model_t *m, double p)
{
  if (!m || !isfinite (p) || p < 0)
    return NAN;
  double v = m->intercept + m->slope * p;
  return isfinite (v) ? fmax (0, v) : NAN;
}
#endif
#endif
