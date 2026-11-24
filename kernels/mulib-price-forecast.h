/** @file mulib-price-forecast.h
 * @brief Uniform forecasting API with naive, EMA, drift and seasonal
 * baselines. C11 single-header module. Define MULIB_IMPL_PRICE_FORECAST in
 * exactly one C translation unit.
 *
 */
#ifndef MULIB_PRICE_FORECAST_H
#define MULIB_PRICE_FORECAST_H
#include "mulib-price-timeseries.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum mulib_price_forecast_model
  {
    MULIB_PRICE_FORECAST_NAIVE,
    MULIB_PRICE_FORECAST_EMA,
    MULIB_PRICE_FORECAST_DRIFT,
    MULIB_PRICE_FORECAST_SEASONAL
  } mulib_price_forecast_model_t;
  /** @brief See manual/mulib-price-forecast.md for this operation's contract.
   */
  int mulib_price_forecast (const double *series, size_t n, size_t horizon,
                            mulib_price_forecast_model_t model, double alpha,
                            size_t season, double *out);
  /** @brief See manual/mulib-price-forecast.md for this operation's contract.
   */
  int mulib_price_forecast_mae (const double *series, size_t n,
                                mulib_price_forecast_model_t model,
                                double alpha, size_t season, double *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_FORECAST_H */
#ifdef MULIB_IMPL_PRICE_FORECAST
#ifndef MULIB_IMPL_PRICE_FORECAST_ONCE
#define MULIB_IMPL_PRICE_FORECAST_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_price_forecast (const double *s, size_t n, size_t h,
                      mulib_price_forecast_model_t m, double a, size_t period,
                      double *out)
{
  if (!s || !out || !n || !h || h > 1000000)
    return 0;
  for (size_t i = 0; i < n; i++)
    if (!isfinite (s[i]) || s[i] < 0)
      return 0;
  double v = s[n - 1];
  switch (m)
    {
    case MULIB_PRICE_FORECAST_NAIVE:
      break;
    case MULIB_PRICE_FORECAST_EMA:
      if (!(a > 0 && a <= 1))
        return 0;
      v = s[0];
      for (size_t i = 1; i < n; i++)
        v = a * s[i] + (1 - a) * v;
      break;
    case MULIB_PRICE_FORECAST_DRIFT:
      if (n < 2)
        return 0;
      v = s[n - 1] + (s[n - 1] - s[0]) / (double)(n - 1) * (double)h;
      break;
    case MULIB_PRICE_FORECAST_SEASONAL:
      if (!period || period > n)
        return 0;
      v = s[n - period + (h - 1) % period];
      break;
    default:
      return 0;
    }
  if (!isfinite (v))
    return 0;
  *out = v < 0 ? 0 : v;
  return 1;
}
int
mulib_price_forecast_mae (const double *s, size_t n,
                          mulib_price_forecast_model_t m, double a,
                          size_t period, double *o)
{
  if (!s || !o || n < 3)
    return 0;
  for (size_t i = 0; i < n; i++)
    if (!isfinite (s[i]) || s[i] < 0)
      return 0;
  size_t first = m == MULIB_PRICE_FORECAST_SEASONAL ? period : 2;
  if (!first || first >= n)
    return 0;
  long double e = 0;
  for (size_t i = first; i < n; i++)
    {
      double v;
      if (!mulib_price_forecast (s, i, 1, m, a, period, &v))
        return 0;
      e += fabsl ((long double)v - s[i]);
    }
  double result = (double)(e / (n - first));
  if (!isfinite (result))
    return 0;
  *o = result;
  return 1;
}
#endif
#endif
