/** @file mulib-pearson_similarity.h
 * @brief Pearson correlation using scaled centered coordinates.
 * Define MULIB_IMPL_PEARSON_SIMILARITY; link libm.
 */
#ifndef MULIB_PEARSON_SIMILARITY_H
#define MULIB_PEARSON_SIMILARITY_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Return correlation in [-1,1]; NAN for n<2, constant vectors or invalid data. */
double mulib_pearson_similarity(const double *a, const double *b, size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PEARSON_SIMILARITY) && !defined(MULIB_PEARSON_SIMILARITY_IMPLEMENTED)
#define MULIB_PEARSON_SIMILARITY_IMPLEMENTED
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
double mulib_pearson_similarity(const double *a, const double *b, size_t n)
{
  double sa = 0, sb = 0;
  long double ma = 0, mb = 0, aa = 0, bb = 0, ab = 0;
  if (!a || !b || n < 2) return NAN;
  for (size_t i = 0; i < n; ++i) {
    if (!isfinite(a[i]) || !isfinite(b[i])) return NAN;
    sa = fmax(sa, fabs(a[i])); sb = fmax(sb, fabs(b[i]));
  }
  if (!sa || !sb) return NAN;
  for (size_t i = 0; i < n; ++i) { ma += (long double)a[i] / sa; mb += (long double)b[i] / sb; }
  ma /= n; mb /= n;
  for (size_t i = 0; i < n; ++i) {
    long double x = (long double)a[i] / sa - ma, y = (long double)b[i] / sb - mb;
    aa += x * x; bb += y * y; ab += x * y;
  }
  if (!aa || !bb) return NAN;
  return (double)fmaxl(-1, fminl(1, ab / sqrtl(aa) / sqrtl(bb)));
}
#ifdef __cplusplus
}
#endif
#endif
