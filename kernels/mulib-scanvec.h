/* MuLIB: Selection-vector numeric predicates with explicit per-call capacity. C11
 * single-header; define MULIB_IMPL_SCANVEC once. */
#ifndef MULIB_SCANVEC_H
#define MULIB_SCANVEC_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum
  {
    MULIB_SCAN_EQ,
    MULIB_SCAN_NE,
    MULIB_SCAN_LT,
    MULIB_SCAN_LE,
    MULIB_SCAN_GT,
    MULIB_SCAN_GE
  } mulib_scan_predicate_t;
  /* Returns count; SIZE_MAX if invalid or output insufficient (no write). NaN never
   * matches. */
  size_t mulib_scanvec_double (const double *values, size_t count,
                               mulib_scan_predicate_t pred, double rhs, size_t *indices,
                               size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SCANVEC) && !defined(MULIB_SCANVEC_IMPLEMENTED)
#define MULIB_SCANVEC_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_scanvec_test_ (double x, double y, mulib_scan_predicate_t p)
  {
    if (isnan (x) || isnan (y))
      return 0;
    switch (p)
      {
      case MULIB_SCAN_EQ:
        return x == y;
      case MULIB_SCAN_NE:
        return x != y;
      case MULIB_SCAN_LT:
        return x < y;
      case MULIB_SCAN_LE:
        return x <= y;
      case MULIB_SCAN_GT:
        return x > y;
      case MULIB_SCAN_GE:
        return x >= y;
      default:
        return 0;
      }
  }
  size_t
  mulib_scanvec_double (const double *v, size_t n, mulib_scan_predicate_t pred,
                        double rhs, size_t *out, size_t cap)
  {
    if ((!v && n) || (unsigned)pred > MULIB_SCAN_GE || (!out && cap))
      return SIZE_MAX;
    size_t found = 0;
    for (size_t i = 0; i < n; i++)
      found += mulib_scanvec_test_ (v[i], rhs, pred);
    if (found > cap)
      return SIZE_MAX;
    for (size_t i = 0, j = 0; i < n; i++)
      if (mulib_scanvec_test_ (v[i], rhs, pred))
        out[j++] = i;
    return found;
  }
#ifdef __cplusplus
}
#endif
#endif
