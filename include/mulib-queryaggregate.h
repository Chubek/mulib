/* MuLIB: Streaming count/sum/min/max/mean aggregates on finite double inputs. C11
 * single-header; define MULIB_IMPL_QUERYAGGREGATE once. */
#ifndef MULIB_QUERYAGGREGATE_H
#define MULIB_QUERYAGGREGATE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    size_t count;
    double sum, min, max, mean;
  } mulib_queryaggregate_t;
  int mulib_queryaggregate_add (mulib_queryaggregate_t *a, double value);
  int mulib_queryaggregate_merge (mulib_queryaggregate_t *dst,
                                  const mulib_queryaggregate_t *src);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_QUERYAGGREGATE) && !defined(MULIB_QUERYAGGREGATE_IMPLEMENTED)
#define MULIB_QUERYAGGREGATE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_queryaggregate_add (mulib_queryaggregate_t *a, double x)
  {
    if (!a || !isfinite (x) || a->count == SIZE_MAX || !isfinite (a->sum + x))
      return 0;
    if (!a->count)
      {
        a->min = a->max = x;
      }
    else
      {
        if (x < a->min)
          a->min = x;
        if (x > a->max)
          a->max = x;
      }
    a->count++;
    a->sum += x;
    a->mean = a->sum / (double)a->count;
    return 1;
  }
  int
  mulib_queryaggregate_merge (mulib_queryaggregate_t *a,
                              const mulib_queryaggregate_t *b)
  {
    if (!a || !b || a == b || a->count > SIZE_MAX - b->count
        || !isfinite (a->sum + b->sum))
      return 0;
    if (!b->count)
      return 1;
    if (!a->count)
      {
        *a = *b;
        return 1;
      }
    size_t total = a->count + b->count;
    a->count = total;
    a->sum += b->sum;
    if (b->min < a->min)
      a->min = b->min;
    if (b->max > a->max)
      a->max = b->max;
    a->mean = a->sum / (double)total;
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
