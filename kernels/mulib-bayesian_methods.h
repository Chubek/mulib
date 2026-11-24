/** @file mulib-bayesian_methods.h
 * @brief Stable finite-hypothesis Bayesian updating using log likelihoods.
 * Define MULIB_IMPL_BAYESIAN_METHODS and MULIB_IMPL_PROBABILITY. Link libm.
 */
#ifndef MULIB_BAYESIAN_METHODS_H
#define MULIB_BAYESIAN_METHODS_H
#include "mulib-probability.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Update a normalized prior; log likelihoods must be <= 0 (including -INFINITY).
 * Output may alias prior, not likelihoods. Failure leaves output unchanged.
 * Zero evidence fails. Optional log_evidence receives the normalization constant.
 */
int mulib_bayesian_update(const double *prior, const double *log_likelihood,
                         size_t n, double *posterior, double *log_evidence);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BAYESIAN_METHODS) && !defined(MULIB_BAYESIAN_METHODS_IMPLEMENTED)
#define MULIB_BAYESIAN_METHODS_IMPLEMENTED
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_bayesian_update(const double *prior, const double *ll, size_t n,
                         double *out, double *evidence)
{
  double *work, z, maximum = -INFINITY;
  if (!out || !ll || !n || n > SIZE_MAX / sizeof(double)
      || !mulib_probability_valid(prior, n, 1e-9)) return 0;
  for (size_t i = 0; i < n; ++i) {
    if (isnan(ll[i]) || ll[i] > 0) return 0;
    if (prior[i] > 0 && ll[i] > maximum) maximum = ll[i];
  }
  if (!isfinite(maximum)) return 0;
  work = (double *)malloc(n * sizeof(*work));
  if (!work) return 0;
  for (size_t i = 0; i < n; ++i)
    work[i] = prior[i] ? log(prior[i]) + (ll[i] - maximum) : -INFINITY;
  z = mulib_probability_logsumexp(work, n);
  if (!isfinite(z)) { free(work); return 0; }
  for (size_t i = 0; i < n; ++i) work[i] = exp(work[i] - z);
  memcpy(out, work, n * sizeof(*work));
  free(work);
  if (evidence) *evidence = maximum + z;
  return 1;
}
#ifdef __cplusplus
}
#endif
#endif
