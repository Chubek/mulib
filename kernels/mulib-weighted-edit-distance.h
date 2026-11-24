/** @file mulib-weighted-edit-distance.h
 * @brief Weighted edit DP with per-symbol costs; canonical access to legacy byte DP.
 * Define MULIB_IMPL_WEIGHTED_EDIT_DISTANCE and MULIB_IMPL_WEIGHED_EDIT_DISTANE.
 */
#ifndef MULIB_WEIGHTED_EDIT_DISTANCE_H
#define MULIB_WEIGHTED_EDIT_DISTANCE_H
#include "mulib-weighed_edit_distane.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_weighted_edit_operation {
  MULIB_WEIGHTED_INSERT, MULIB_WEIGHTED_DELETE, MULIB_WEIGHTED_SUBSTITUTE
} mulib_weighted_edit_operation_t;
/** Pure callback, finite nonnegative result; unused symbols are zero.
 * Matching symbols always cost zero. Direction transforms a into b. */
typedef double (*mulib_weighted_edit_cost_fn)(mulib_weighted_edit_operation_t,
    uint32_t a, uint32_t b, void *user);
/** Scalar-token DP, O(n*m) time/O(m) space. 1 success, 0 invalid/OOM/work limit.
 * out unchanged on error. max_cells zero selects 10000000. No transpositions. */
int mulib_weighted_edit_distance_ex(const uint32_t *, size_t, const uint32_t *, size_t,
    mulib_weighted_edit_cost_fn, void *, size_t max_cells, double *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WEIGHTED_EDIT_DISTANCE) && !defined(MULIB_WEIGHTED_EDIT_DISTANCE_IMPLEMENTED)
#define MULIB_WEIGHTED_EDIT_DISTANCE_IMPLEMENTED
#include <math.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_weighted_edit_distance_ex(const uint32_t *a, size_t n, const uint32_t *b, size_t m,
    mulib_weighted_edit_cost_fn cost, void *u, size_t limit, double *out) {
  double *row, result = 0;
  size_t i, j;
  if (!limit) limit = 10000000;
  if ((!a && n) || (!b && m) || !cost || !out || m >= SIZE_MAX / sizeof *row ||
      n == SIZE_MAX || m == SIZE_MAX || n + 1 > limit / (m + 1)) return 0;
  row = (double *)malloc((m + 1) * sizeof *row);
  if (!row) return 0;
  row[0] = 0;
  for (j = 1; j <= m; ++j) {
    double c = cost(MULIB_WEIGHTED_INSERT, 0, b[j - 1], u);
    if (!isfinite(c) || c < 0 || !isfinite(row[j] = row[j - 1] + c)) goto fail;
  }
  for (i = 1; i <= n; ++i) {
    double d = cost(MULIB_WEIGHTED_DELETE, a[i - 1], 0, u), diagonal = row[0];
    if (!isfinite(d) || d < 0 || !isfinite(row[0] += d)) goto fail;
    for (j = 1; j <= m; ++j) {
      double ins = cost(MULIB_WEIGHTED_INSERT, 0, b[j - 1], u);
      double sub = a[i - 1] == b[j - 1] ? 0 : cost(MULIB_WEIGHTED_SUBSTITUTE, a[i - 1], b[j - 1], u);
      double old = row[j];
      if (!isfinite(ins) || ins < 0 || !isfinite(sub) || sub < 0) goto fail;
      row[j] = fmin(old + d, fmin(row[j - 1] + ins, diagonal + sub));
      if (!isfinite(row[j])) goto fail;
      diagonal = old;
    }
  }
  result = row[m]; free(row); *out = result; return 1;
fail:
  free(row); return 0;
}
#ifdef __cplusplus
}
#endif
#endif
