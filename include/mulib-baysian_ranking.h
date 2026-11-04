/** @file mulib-baysian_ranking.h
 * @brief Beta-Bernoulli posterior-mean scores and deterministic ranking.
 * Define MULIB_IMPL_BAYSIAN_RANKING. Historical filename retained.
 */
#ifndef MULIB_BAYSIAN_RANKING_H
#define MULIB_BAYSIAN_RANKING_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Score (successes+alpha)/(trials+alpha+beta); NAN for invalid input. */
double mulib_bayesian_rank_score(double successes, double trials, double alpha, double beta);
/** Rank finite scores descending, ties by original index; indices has n slots.
 * Output is unchanged on invalid input; scores and indices must not overlap. */
int mulib_bayesian_rank(const double *scores, size_t n, size_t *indices);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BAYSIAN_RANKING) && !defined(MULIB_BAYSIAN_RANKING_IMPLEMENTED)
#define MULIB_BAYSIAN_RANKING_IMPLEMENTED
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
double mulib_bayesian_rank_score(double s, double n, double a, double b)
{
  double scale;
  if (!isfinite(s) || !isfinite(n) || !isfinite(a) || !isfinite(b)
      || s < 0 || n < s || a <= 0 || b <= 0) return NAN;
  scale = fmax(n, fmax(a, b));
  return (s / scale + a / scale) / (n / scale + a / scale + b / scale);
}
int mulib_bayesian_rank(const double *s, size_t n, size_t *out)
{
  if ((!s || !out) && n) return 0;
  for (size_t i = 0; i < n; ++i) if (!isfinite(s[i])) return 0;
  for (size_t i = 0; i < n; ++i) {
    size_t j = i;
    while (j && s[out[j - 1]] < s[i]) { out[j] = out[j - 1]; --j; }
    out[j] = i;
  }
  return 1;
}
#ifdef __cplusplus
}
#endif
#endif
