/** @file mulib-euclidean_distance.h
 * @brief Euclidean distance with hypot accumulation.
 * Define MULIB_IMPL_EUCLIDEAN_DISTANCE; link libm.
 */
#ifndef MULIB_EUCLIDEAN_DISTANCE_H
#define MULIB_EUCLIDEAN_DISTANCE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Empty vectors give zero. Invalid/nonfinite input gives NAN; overflow gives INFINITY. */
double mulib_euclidean_distance(const double *a, const double *b, size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EUCLIDEAN_DISTANCE) && !defined(MULIB_EUCLIDEAN_DISTANCE_IMPLEMENTED)
#define MULIB_EUCLIDEAN_DISTANCE_IMPLEMENTED
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
double mulib_euclidean_distance(const double *a, const double *b, size_t n)
{
  long double d = 0;
  if ((!a || !b) && n) return NAN;
  for (size_t i = 0; i < n; ++i) {
    if (!isfinite(a[i]) || !isfinite(b[i])) return NAN;
    d = hypotl(d, (long double)a[i] - b[i]);
  }
  return (double)d;
}
#ifdef __cplusplus
}
#endif
#endif
