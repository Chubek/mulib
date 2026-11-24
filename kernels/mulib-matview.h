/* MuLIB: Incrementally maintained count/sum materialized numeric aggregate view. C11
 * single-header; define MULIB_IMPL_MATVIEW once. */
#ifndef MULIB_MATVIEW_H
#define MULIB_MATVIEW_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t rows;
    double sum;
  } mulib_matview_t;
  int mulib_matview_insert (mulib_matview_t *v, double value);
  int mulib_matview_delete (mulib_matview_t *v, double value);
  double mulib_matview_avg (const mulib_matview_t *v);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MATVIEW) && !defined(MULIB_MATVIEW_IMPLEMENTED)
#define MULIB_MATVIEW_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_matview_insert (mulib_matview_t *v, double x)
  {
    if (!v || !isfinite (x) || v->rows == UINT64_MAX || !isfinite (v->sum + x))
      return 0;
    v->rows++;
    v->sum += x;
    return 1;
  }
  int
  mulib_matview_delete (mulib_matview_t *v, double x)
  {
    if (!v || !isfinite (x) || !v->rows || !isfinite (v->sum - x))
      return 0;
    v->rows--;
    v->sum = v->rows ? v->sum - x : 0;
    return 1;
  }
  double
  mulib_matview_avg (const mulib_matview_t *v)
  {
    return v && v->rows ? v->sum / (double)v->rows : NAN;
  }
#ifdef __cplusplus
}
#endif
#endif
