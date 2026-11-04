/** @file mulib-jaccard_similarity.h
 * @brief Jaccard similarity of uint64 sets, ignoring repeated elements.
 * Define MULIB_IMPL_JACCARD_SIMILARITY. Quadratic time, constant storage.
 */
#ifndef MULIB_JACCARD_SIMILARITY_H
#define MULIB_JACCARD_SIMILARITY_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Intersection/union; two empty sets give 1. NAN for invalid pointers. */
double mulib_jaccard_similarity(const uint64_t *a, size_t n, const uint64_t *b, size_t m);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_JACCARD_SIMILARITY) && !defined(MULIB_JACCARD_SIMILARITY_IMPLEMENTED)
#define MULIB_JACCARD_SIMILARITY_IMPLEMENTED
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_jaccard_contains_(const uint64_t *a, size_t n, uint64_t v)
{
  for (size_t i = 0; i < n; ++i) if (a[i] == v) return 1;
  return 0;
}
double mulib_jaccard_similarity(const uint64_t *a, size_t n, const uint64_t *b, size_t m)
{
  long double intersection = 0, total = 0;
  if ((!a && n) || (!b && m)) return NAN;
  for (size_t i = 0; i < n; ++i) if (!mulib_jaccard_contains_(a, i, a[i])) {
    ++total;
    intersection += mulib_jaccard_contains_(b, m, a[i]);
  }
  for (size_t i = 0; i < m; ++i)
    if (!mulib_jaccard_contains_(b, i, b[i]) && !mulib_jaccard_contains_(a, n, b[i])) ++total;
  return total ? (double)(intersection / total) : 1;
}
#ifdef __cplusplus
}
#endif
#endif
