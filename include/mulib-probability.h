/** @file mulib-probability.h
 * @brief Discrete probability validation, normalization, entropy and log sums.
 * Define MULIB_IMPL_PROBABILITY in one C translation unit. Link libm.
 */
#ifndef MULIB_PROBABILITY_H
#define MULIB_PROBABILITY_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Validate nonnegative finite entries summing to one within tolerance. */
int mulib_probability_valid(const double *p, size_t n, double tolerance);
/** Normalize finite nonnegative weights in place; failure leaves them unchanged. */
int mulib_probability_normalize(double *weights, size_t n);
/** Shannon entropy in nats; NAN for an invalid distribution. */
double mulib_probability_entropy(const double *p, size_t n);
/** Log(sum(exp(x))); empty input gives -INFINITY, invalid/NaN gives NAN. */
double mulib_probability_logsumexp(const double *x, size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PROBABILITY) && !defined(MULIB_PROBABILITY_IMPLEMENTED)
#define MULIB_PROBABILITY_IMPLEMENTED
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_probability_valid(const double *p, size_t n, double tolerance)
{
  long double sum = 0;
  if (!p || !n || !isfinite(tolerance) || tolerance < 0) return 0;
  for (size_t i = 0; i < n; ++i) {
    if (!isfinite(p[i]) || p[i] < 0 || p[i] > 1) return 0;
    sum += p[i];
  }
  return fabsl(sum - 1) <= tolerance;
}
int mulib_probability_normalize(double *p, size_t n)
{
  double maximum = 0;
  long double sum = 0;
  if (!p || !n) return 0;
  for (size_t i = 0; i < n; ++i) {
    if (!isfinite(p[i]) || p[i] < 0) return 0;
    if (p[i] > maximum) maximum = p[i];
  }
  if (!maximum) return 0;
  for (size_t i = 0; i < n; ++i) sum += p[i] / maximum;
  for (size_t i = 0; i < n; ++i) p[i] = (double)((p[i] / maximum) / sum);
  return 1;
}
double mulib_probability_entropy(const double *p, size_t n)
{
  long double h = 0;
  if (!mulib_probability_valid(p, n, 1e-9)) return NAN;
  for (size_t i = 0; i < n; ++i) if (p[i] > 0) h -= p[i] * log(p[i]);
  return (double)h;
}
double mulib_probability_logsumexp(const double *x, size_t n)
{
  double maximum = -INFINITY;
  long double sum = 0;
  if (!x && n) return NAN;
  for (size_t i = 0; i < n; ++i) {
    if (isnan(x[i])) return NAN;
    if (x[i] > maximum) maximum = x[i];
  }
  if (!isfinite(maximum)) return maximum;
  for (size_t i = 0; i < n; ++i) sum += exp(x[i] - maximum);
  return maximum + (double)logl(sum);
}
#ifdef __cplusplus
}
#endif
#endif
