/** @file mulib-edit_distance.h
 * @brief Unit-cost byte edit distance for binary buffers and C strings.
 * Define MULIB_IMPL_EDIT_DISTANCE and MULIB_IMPL_LEVENSHTEIN.
 */
#ifndef MULIB_EDIT_DISTANCE_H
#define MULIB_EDIT_DISTANCE_H
#include "mulib-levenshtein.h"
#ifdef __cplusplus
extern "C" {
#endif
/** C-string distance; SIZE_MAX on invalid input or allocation failure. */
size_t mulib_edit_distance(const char *a, const char *b);
/** Binary-buffer distance, O(n*m) time and O(min(n,m)) storage.
 * NULL is permitted for zero lengths. SIZE_MAX indicates failure. */
size_t mulib_edit_distance_n(const void *a, size_t n, const void *b, size_t m);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDIT_DISTANCE) && !defined(MULIB_EDIT_DISTANCE_IMPLEMENTED)
#define MULIB_EDIT_DISTANCE_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_edit_distance(const char *a, const char *b)
{ return mulib_levenshtein_distance(a, b); }
size_t mulib_edit_distance_n(const void *a, size_t n, const void *b, size_t m)
{
  const unsigned char *x = (const unsigned char *)a, *y = (const unsigned char *)b;
  size_t *row, result;
  if ((!a && n) || (!b && m) || n == SIZE_MAX || m == SIZE_MAX) return SIZE_MAX;
  if (m > n) return mulib_edit_distance_n(b, m, a, n);
  if (!m) return n;
  if (m >= SIZE_MAX / sizeof(*row)) return SIZE_MAX;
  row = (size_t *)malloc((m + 1) * sizeof(*row));
  if (!row) return SIZE_MAX;
  for (size_t j = 0; j <= m; ++j) row[j] = j;
  for (size_t i = 1; i <= n; ++i) {
    size_t diagonal = row[0];
    row[0] = i;
    for (size_t j = 1; j <= m; ++j) {
      size_t old = row[j], v = diagonal + (x[i - 1] != y[j - 1]);
      if (old + 1 < v) v = old + 1;
      if (row[j - 1] + 1 < v) v = row[j - 1] + 1;
      row[j] = v; diagonal = old;
    }
  }
  result = row[m]; free(row); return result;
}
#ifdef __cplusplus
}
#endif
#endif
