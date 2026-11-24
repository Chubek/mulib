/* MuLIB: Min/max/null statistics for numeric column pruning. C11 single-header; define
 * MULIB_IMPL_COLUMNSTATS once. */
#ifndef MULIB_COLUMNSTATS_H
#define MULIB_COLUMNSTATS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t values, nulls;
    double min, max;
  } mulib_columnstats_t;
  int mulib_columnstats_add (mulib_columnstats_t *s, double value, int valid);
  /* Returns true if [min,max] could contain the closed predicate range. */
  int mulib_columnstats_may_match (const mulib_columnstats_t *s, double low,
                                   double high);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_COLUMNSTATS) && !defined(MULIB_COLUMNSTATS_IMPLEMENTED)
#define MULIB_COLUMNSTATS_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_columnstats_add (mulib_columnstats_t *s, double x, int valid)
  {
    if (!s || s->values == UINT64_MAX || s->nulls == UINT64_MAX
        || (valid && !isfinite (x)))
      return 0;
    if (!valid)
      {
        s->nulls++;
        return 1;
      }
    if (!s->values)
      s->min = s->max = x;
    else
      {
        if (x < s->min)
          s->min = x;
        if (x > s->max)
          s->max = x;
      }
    s->values++;
    return 1;
  }
  int
  mulib_columnstats_may_match (const mulib_columnstats_t *s, double low, double high)
  {
    return s && s->values && isfinite (low) && isfinite (high) && low <= high
           && s->max >= low && s->min <= high;
  }
#ifdef __cplusplus
}
#endif
#endif
