/** @file mulib-hidden_markov_chain.h
 * @brief Discrete-emission hidden Markov models with scaled forward inference.
 * Define MULIB_IMPL_HIDDEN_MARKOV_CHAIN, MULIB_IMPL_MARKOV_CHAIN and
 * MULIB_IMPL_PROBABILITY. Link libm. Model arrays are borrowed, never modified.
 */
#ifndef MULIB_HIDDEN_MARKOV_CHAIN_H
#define MULIB_HIDDEN_MARKOV_CHAIN_H
#include "mulib-markov_chain.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_hidden_markov_chain {
  size_t states, symbols;
  const double *initial, *transition, *emission;
} mulib_hidden_markov_chain_t;
/** Validate initial distribution, transition rows and states*symbols emission rows. */
int mulib_hidden_markov_chain_valid(const mulib_hidden_markov_chain_t *model);
/** Log observation likelihood (scaled forward algorithm).
 * Empty observations give 0; impossible observations give -INFINITY, both success.
 * Return 0 on invalid/OOM, preserving output. */
int mulib_hidden_markov_chain_forward(const mulib_hidden_markov_chain_t *model,
                                     const size_t *observations, size_t length, double *log_likelihood);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HIDDEN_MARKOV_CHAIN) && !defined(MULIB_HIDDEN_MARKOV_CHAIN_IMPLEMENTED)
#define MULIB_HIDDEN_MARKOV_CHAIN_IMPLEMENTED
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hidden_markov_chain_valid(const mulib_hidden_markov_chain_t *m)
{
  mulib_markov_chain_t c;
  if (!m || !m->symbols || !m->states || !m->emission || m->states > SIZE_MAX / m->symbols
      || m->states * m->symbols > SIZE_MAX / sizeof(double)) return 0;
  c.states = m->states; c.transition = m->transition;
  if (!mulib_markov_chain_valid(&c) || !mulib_probability_valid(m->initial, m->states, 1e-9)) return 0;
  for (size_t i = 0; i < m->states; ++i)
    if (!mulib_probability_valid(m->emission + i * m->symbols, m->symbols, 1e-9)) return 0;
  return 1;
}
int mulib_hidden_markov_chain_forward(const mulib_hidden_markov_chain_t *m,
                                     const size_t *obs, size_t length, double *out)
{
  long double *a, *b, logp = 0;
  size_t n;
  if (!out || (!obs && length) || !mulib_hidden_markov_chain_valid(m)) return 0;
  for (size_t t = 0; t < length; ++t) if (obs[t] >= m->symbols) return 0;
  if (!length) { *out = 0; return 1; }
  n = m->states;
  if (n > SIZE_MAX / (2 * sizeof(*a))) return 0;
  a = (long double *)malloc(2 * n * sizeof(*a));
  if (!a) return 0;
  b = a + n;
  for (size_t t = 0; t < length; ++t) {
    long double scale = 0;
    for (size_t j = 0; j < n; ++j) {
      long double p = t ? 0 : m->initial[j];
      if (t) for (size_t i = 0; i < n; ++i) p += a[i] * m->transition[i * n + j];
      b[j] = p * m->emission[j * m->symbols + obs[t]]; scale += b[j];
    }
    if (!scale) { free(a); *out = -INFINITY; return 1; }
    logp += logl(scale);
    for (size_t j = 0; j < n; ++j) a[j] = b[j] / scale;
  }
  free(a); *out = (double)logp; return 1;
}
#ifdef __cplusplus
}
#endif
#endif
