/** @file mulib-weighed_edit_distane.h
 * @brief Weighted byte edit distance with directional insertion/deletion costs.
 * Define MULIB_IMPL_WEIGHED_EDIT_DISTANE; link libm. Filename retained.
 */
#ifndef MULIB_WEIGHED_EDIT_DISTANE_H
#define MULIB_WEIGHED_EDIT_DISTANE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Transform a into b, with finite nonnegative costs. Matching costs zero.
 * NULL buffers require zero length. NAN on invalid/OOM; INFINITY on overflow.
 * O(n*m) time, O(m) storage. No transpositions or Unicode segmentation. */
double mulib_weighted_edit_distance(const void *a, size_t n, const void *b, size_t m,
                                   double insertion, double deletion, double substitution);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WEIGHED_EDIT_DISTANE) && !defined(MULIB_WEIGHED_EDIT_DISTANE_IMPLEMENTED)
#define MULIB_WEIGHED_EDIT_DISTANE_IMPLEMENTED
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
double mulib_weighted_edit_distance(const void *a, size_t n, const void *b, size_t m,
                                   double ins, double del, double sub)
{
  const unsigned char *x = (const unsigned char *)a, *y = (const unsigned char *)b;
  double *row, result;
  if ((!a && n) || (!b && m) || n == SIZE_MAX || m >= SIZE_MAX / sizeof(*row)
      || !isfinite(ins) || !isfinite(del) || !isfinite(sub) || ins < 0 || del < 0 || sub < 0) return NAN;
  if (!m) return n * del;
  if (!n) return m * ins;
  row = (double *)malloc((m + 1) * sizeof(*row));
  if (!row) return NAN;
  for (size_t j = 0; j <= m; ++j) row[j] = j * ins;
  for (size_t i = 1; i <= n; ++i) {
    double diagonal = row[0]; row[0] = i * del;
    for (size_t j = 1; j <= m; ++j) {
      double old = row[j];
      row[j] = fmin(old + del, fmin(row[j - 1] + ins,
                                   diagonal + (x[i - 1] == y[j - 1] ? 0 : sub)));
      diagonal = old;
    }
  }
  result = row[m]; free(row); return result;
}
#ifdef __cplusplus
}
#endif
#endif
