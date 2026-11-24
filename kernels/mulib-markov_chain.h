/** @file mulib-markov_chain.h
 * @brief Dense finite-state Markov chains and smoothed transition estimation.
 * Define MULIB_IMPL_MARKOV_CHAIN and MULIB_IMPL_PROBABILITY. Link libm.
 */
#ifndef MULIB_MARKOV_CHAIN_H
#define MULIB_MARKOV_CHAIN_H
#include "mulib-probability.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Borrowed row-major states*states transition matrix. */
typedef struct mulib_markov_chain { size_t states; const double *transition; } mulib_markov_chain_t;
/** Validate dimensions and stochastic rows (tolerance 1e-9). */
int mulib_markov_chain_valid(const mulib_markov_chain_t *chain);
/** Multiply a normalized row distribution by the matrix; output may alias input.
 * Return 0 on invalid/OOM without changing output. */
int mulib_markov_chain_step(const mulib_markov_chain_t *chain, const double *distribution, double *output);
/** Estimate from adjacent state IDs in one sequence with additive smoothing.
 * Output has states*states slots. Unobserved rows without smoothing fail.
 * Zero-length input is allowed; failure leaves output unchanged. */
int mulib_markov_chain_fit(size_t states, const size_t *sequence, size_t length,
                           double smoothing, double *transition);
/** Inverse-CDF sample from a state, given caller-supplied uniform value in [0,1).
 * SIZE_MAX on invalid input. No hidden RNG state. */
size_t mulib_markov_chain_sample(const mulib_markov_chain_t *chain, size_t state, double uniform);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MARKOV_CHAIN) && !defined(MULIB_MARKOV_CHAIN_IMPLEMENTED)
#define MULIB_MARKOV_CHAIN_IMPLEMENTED
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_markov_chain_valid(const mulib_markov_chain_t *c)
{
  if (!c || !c->states || !c->transition || c->states > SIZE_MAX / c->states
      || c->states * c->states > SIZE_MAX / sizeof(double)) return 0;
  for (size_t i = 0; i < c->states; ++i)
    if (!mulib_probability_valid(c->transition + i * c->states, c->states, 1e-9)) return 0;
  return 1;
}
int mulib_markov_chain_step(const mulib_markov_chain_t *c, const double *p, double *out)
{
  double *work;
  if (!out || !mulib_markov_chain_valid(c) || !mulib_probability_valid(p, c->states, 1e-9)) return 0;
  work = (double *)calloc(c->states, sizeof(*work));
  if (!work) return 0;
  for (size_t j = 0; j < c->states; ++j) {
    long double sum = 0;
    for (size_t i = 0; i < c->states; ++i) sum += (long double)p[i] * c->transition[i * c->states + j];
    work[j] = (double)sum;
  }
  memcpy(out, work, c->states * sizeof(*work)); free(work); return 1;
}
int mulib_markov_chain_fit(size_t n, const size_t *s, size_t length, double smoothing, double *out)
{
  double *work;
  if (!out || !n || n > SIZE_MAX / n || n * n > SIZE_MAX / sizeof(double)
      || (!s && length) || !isfinite(smoothing) || smoothing < 0) return 0;
  for (size_t i = 0; i < length; ++i) if (s[i] >= n) return 0;
  work = (double *)malloc(n * n * sizeof(*work));
  if (!work) return 0;
  for (size_t i = 0; i < n * n; ++i) work[i] = smoothing;
  for (size_t i = 1; i < length; ++i) work[s[i - 1] * n + s[i]] += 1;
  for (size_t i = 0; i < n; ++i) if (!mulib_probability_normalize(work + i * n, n)) { free(work); return 0; }
  memcpy(out, work, n * n * sizeof(*work)); free(work); return 1;
}
size_t mulib_markov_chain_sample(const mulib_markov_chain_t *c, size_t state, double u)
{
  long double sum = 0;
  size_t last = SIZE_MAX;
  if (!mulib_markov_chain_valid(c) || state >= c->states || !isfinite(u) || u < 0 || u >= 1) return SIZE_MAX;
  for (size_t j = 0; j < c->states; ++j) {
    double p = c->transition[state * c->states + j];
    if (p > 0) last = j;
    sum += p;
    if (u < sum) return j;
  }
  return last;
}
#ifdef __cplusplus
}
#endif
#endif
