/** @file mulib-price-monitor.h
 * @brief Thresholded price change and availability event detection.
 * C11 single-header module. Define MULIB_IMPL_PRICE_MONITOR in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_PRICE_MONITOR_H
#define MULIB_PRICE_MONITOR_H
#include "mulib-price-observation.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum mulib_price_monitor_event
  {
    MULIB_PRICE_MONITOR_NO_CHANGE = 0,
    MULIB_PRICE_MONITOR_UP = 1,
    MULIB_PRICE_MONITOR_DOWN = 2,
    MULIB_PRICE_MONITOR_STOCK = 3
  } mulib_price_monitor_event_t;
  /** @brief See manual/mulib-price-monitor.md for this operation's contract.
   */
  int mulib_price_monitor_compare (const mulib_price_observation_t *old_item,
                                   const mulib_price_observation_t *new_item,
                                   double min_relative_change,
                                   mulib_price_monitor_event_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRICE_MONITOR_H */
#ifdef MULIB_IMPL_PRICE_MONITOR
#ifndef MULIB_IMPL_PRICE_MONITOR_ONCE
#define MULIB_IMPL_PRICE_MONITOR_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_price_monitor_compare (const mulib_price_observation_t *a,
                             const mulib_price_observation_t *b, double min,
                             mulib_price_monitor_event_t *o)
{
  if (!o || !mulib_price_observation_validate (a)
      || !mulib_price_observation_validate (b)
      || !mulib_price_observation_same_offer (a, b) || !isfinite (min)
      || min < 0)
    return 0;
  mulib_price_monitor_event_t z = MULIB_PRICE_MONITOR_NO_CHANGE;
  if ((a->flags & 1) != (b->flags & 1))
    z = MULIB_PRICE_MONITOR_STOCK;
  else
    {
      long double delta = (long double)b->price.minor - a->price.minor;
      long double ratio
          = fabsl (delta) / fmaxl (1, fabsl ((long double)a->price.minor));
      if (delta && ratio >= min)
        z = delta > 0 ? MULIB_PRICE_MONITOR_UP : MULIB_PRICE_MONITOR_DOWN;
    }
  *o = z;
  return 1;
}
#endif
#endif
