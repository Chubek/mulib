/** @file mulib-price-timeseries.h
 * @brief Time-bucketed price series with missing-value flags and gap handling.
 * C11 single-header module. Define MULIB_IMPL_PRICE_TIMESERIES in exactly one
 * C translation unit.
 *
 */
#ifndef MULIB_PRICE_TIMESERIES_H
#define MULIB_PRICE_TIMESERIES_H
#include "mulib-price-history.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_price_point
  {
    int64_t time;
    double value;
    uint32_t count;
    int observed;
  } mulib_price_point_t;
  /** @brief See manual/mulib-price-timeseries.md for this operation's
   * contract. */
  int mulib_price_timeseries_build (const mulib_price_history_t *h,
                                    int64_t start, int64_t step,
                                    size_t buckets, mulib_price_point_t *out);
  /** @brief See manual/mulib-price-timeseries.md for this operation's
   * contract. */
  int mulib_price_timeseries_interpolate (mulib_price_point_t *series,
                                          size_t n);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_TIMESERIES_H */
#ifdef MULIB_IMPL_PRICE_TIMESERIES
#ifndef MULIB_IMPL_PRICE_TIMESERIES_ONCE
#define MULIB_IMPL_PRICE_TIMESERIES_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_price_timeseries_build (const mulib_price_history_t *h, int64_t start,
                              int64_t step, size_t n, mulib_price_point_t *out)
{
  if (!h || (!out && n) || start < 0 || step <= 0 || n > 1000000
      || (n && n - 1 > (uint64_t)(INT64_MAX - start) / (uint64_t)step))
    return 0;
  for (size_t j = 0; j < h->len; j++)
    if (!mulib_price_observation_validate (h->items + j)
        || (j && !mulib_price_observation_same_offer (h->items, h->items + j)))
      return 0;
  if (!n)
    return 1;
  mulib_price_point_t *tmp = calloc (n, sizeof *tmp);
  if (!tmp)
    return 0;
  for (size_t i = 0; i < n; i++)
    tmp[i].time = start + (int64_t)i * step;
  for (size_t j = 0; j < h->len; j++)
    {
      int64_t t = h->items[j].observed_at;
      if (t < start)
        continue;
      uint64_t k = (uint64_t)(t - start) / (uint64_t)step;
      if (k >= n)
        continue;
      mulib_price_point_t *p = tmp + k;
      if (p->count == UINT32_MAX)
        {
          free (tmp);
          return 0;
        }
      p->count++;
      p->value += ((double)h->items[j].price.minor - p->value) / p->count;
      p->observed = 1;
    }
  memcpy (out, tmp, n * sizeof *tmp);
  free (tmp);
  return 1;
}
int
mulib_price_timeseries_interpolate (mulib_price_point_t *p, size_t n)
{
  if (!p || !n)
    return 0;
  size_t first = n;
  for (size_t i = 0; i < n; i++)
    {
      if ((i && p[i].time <= p[i - 1].time)
          || (p[i].observed && (!isfinite (p[i].value) || p[i].value < 0)))
        return 0;
      if (p[i].observed && first == n)
        first = i;
    }
  if (first == n)
    return 0;
  for (size_t i = 0; i < first; i++)
    p[i].value = p[first].value;
  size_t prev = first;
  for (size_t i = first + 1; i < n; i++)
    if (p[i].observed)
      {
        for (size_t j = prev + 1; j < i; j++)
          {
            long double fraction = ((long double)p[j].time - p[prev].time)
                                   / ((long double)p[i].time - p[prev].time);
            p[j].value = (double)((1 - fraction) * p[prev].value
                                  + fraction * p[i].value);
          }
        prev = i;
      }
  for (size_t i = prev + 1; i < n; i++)
    p[i].value = p[prev].value;
  return 1;
}
#endif
#endif
