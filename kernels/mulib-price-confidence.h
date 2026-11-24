/** @file mulib-price-confidence.h
 * @brief Weighted confidence primitives and median/MAD deviation scores.
 * C11 single-header module. Define MULIB_IMPL_PRICE_CONFIDENCE in exactly one
 * C translation unit.
 *
 */
#ifndef MULIB_PRICE_CONFIDENCE_H
#define MULIB_PRICE_CONFIDENCE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-price-confidence.md for this operation's
   * contract. */
  int mulib_price_confidence_combine (const double *scores,
                                      const double *weights, size_t n,
                                      double *out);
  /** @brief See manual/mulib-price-confidence.md for this operation's
   * contract. */
  int mulib_price_confidence_deviation (const double *prices, size_t n,
                                        double candidate, double *score);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_CONFIDENCE_H */
#ifdef MULIB_IMPL_PRICE_CONFIDENCE
#ifndef MULIB_IMPL_PRICE_CONFIDENCE_ONCE
#define MULIB_IMPL_PRICE_CONFIDENCE_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_price_confidence_combine (const double *s, const double *w, size_t n,
                                double *o)
{
  if (!s || !w || !o || !n)
    return 0;
  double max = 0;
  for (size_t i = 0; i < n; i++)
    {
      if (!isfinite (s[i]) || s[i] < 0 || s[i] > 1 || !isfinite (w[i])
          || w[i] < 0)
        return 0;
      if (w[i] > max)
        max = w[i];
    }
  if (!max)
    return 0;
  long double t = 0, z = 0;
  for (size_t i = 0; i < n; i++)
    {
      long double weight = (long double)w[i] / max;
      t += s[i] * weight;
      z += weight;
    }
  double result = (double)(t / z);
  if (!isfinite (result))
    return 0;
  *o = result;
  return 1;
}
static int
mulib_price_confidence_cmp_ (const void *a, const void *b)
{
  double x = *(const double *)a, y = *(const double *)b;
  return (x > y) - (x < y);
}
int
mulib_price_confidence_deviation (const double *p, size_t n, double candidate,
                                  double *o)
{
  if (!p || !n || !o || !isfinite (candidate)
      || n > SIZE_MAX / sizeof (double))
    return 0;
  double *a = malloc (n * sizeof (double));
  if (!a)
    return 0;
  for (size_t i = 0; i < n; i++)
    {
      if (!isfinite (p[i]))
        {
          free (a);
          return 0;
        }
      a[i] = p[i];
    }
  qsort (a, n, sizeof (double), mulib_price_confidence_cmp_);
  double med = n % 2 ? a[n / 2] : a[n / 2 - 1] * .5 + a[n / 2] * .5;
  for (size_t i = 0; i < n; i++)
    {
      a[i] = (double)fabsl ((long double)p[i] - med);
      if (!isfinite (a[i]))
        {
          free (a);
          return 0;
        }
    }
  qsort (a, n, sizeof (double), mulib_price_confidence_cmp_);
  double mad = n % 2 ? a[n / 2] : a[n / 2 - 1] * .5 + a[n / 2] * .5;
  free (a);
  double result = (double)(fabsl ((long double)candidate - med)
                           / (1.4826L * mad + 1e-9L));
  if (!isfinite (result))
    return 0;
  *o = result;
  return 1;
}
#endif
#endif
