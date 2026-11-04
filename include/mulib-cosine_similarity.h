/** @file mulib-cosine_similarity.h
 * @brief Scale-safe cosine similarity of finite double vectors.
 * Define MULIB_IMPL_COSINE_SIMILARITY; link libm.
 */
#ifndef MULIB_COSINE_SIMILARITY_H
#define MULIB_COSINE_SIMILARITY_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Two zero vectors (including empty) give 1, one zero vector gives 0.
 * Invalid pointers or nonfinite coordinates give NAN. */
double mulib_cosine_similarity(const double *a, const double *b, size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_COSINE_SIMILARITY) && !defined(MULIB_COSINE_SIMILARITY_IMPLEMENTED)
#define MULIB_COSINE_SIMILARITY_IMPLEMENTED
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
double mulib_cosine_similarity(const double *a, const double *b, size_t n)
{
  double sa = 0, sb = 0;
  long double dot = 0, aa = 0, bb = 0, value;
  if ((!a || !b) && n) return NAN;
  for (size_t i = 0; i < n; ++i) {
    if (!isfinite(a[i]) || !isfinite(b[i])) return NAN;
    sa = fmax(sa, fabs(a[i])); sb = fmax(sb, fabs(b[i]));
  }
  if (!sa || !sb) return sa == sb ? 1 : 0;
  for (size_t i = 0; i < n; ++i) {
    long double x = (long double)a[i] / sa, y = (long double)b[i] / sb;
    dot += x * y; aa += x * x; bb += y * y;
  }
  value = dot / sqrtl(aa) / sqrtl(bb);
  return (double)fmaxl(-1, fminl(1, value));
}
#ifdef __cplusplus
}
#endif
#endif
