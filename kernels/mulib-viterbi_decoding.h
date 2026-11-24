/** @file mulib-viterbi_decoding.h
 * @brief Log-space Viterbi decoding with deterministic state ties.
 * Define MULIB_IMPL_VITERBI_DECODING and the hidden-Markov dependency switches.
 */
#ifndef MULIB_VITERBI_DECODING_H
#define MULIB_VITERBI_DECODING_H
#include "mulib-hidden_markov_chain.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Best state path and joint log probability. path has length slots, may be NULL
 * for length zero. Return 1 on success, 0 invalid/OOM/impossible; outputs unchanged.
 * Ties choose lowest predecessor and final state IDs. */
int mulib_viterbi_decode(const mulib_hidden_markov_chain_t *model, const size_t *observations,
                        size_t length, size_t *path, double *log_probability);
/** State-pruned decoding: retain best width states at each time step.
 * width>=states is exact. width must be positive. Used by beam_search. */
int mulib_viterbi_decode_beam(const mulib_hidden_markov_chain_t *model, const size_t *observations,
                             size_t length, size_t width, size_t *path, double *log_probability);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VITERBI_DECODING) && !defined(MULIB_VITERBI_DECODING_IMPLEMENTED)
#define MULIB_VITERBI_DECODING_IMPLEMENTED
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
static double mulib_viterbi_log_(double p) { return p > 0 ? log(p) : -INFINITY; }
int mulib_viterbi_decode_beam(const mulib_hidden_markov_chain_t *m, const size_t *obs,
                             size_t length, size_t width, size_t *path, double *out)
{
  double *a, *b, best;
  size_t *back, *order, n, state = 0;
  if (!out || (!path && length) || (!obs && length) || !width || !mulib_hidden_markov_chain_valid(m)) return 0;
  for (size_t t = 0; t < length; ++t) if (obs[t] >= m->symbols) return 0;
  if (!length) { *out = 0; return 1; }
  n = m->states;
  if (!n || n > SIZE_MAX / (2 * sizeof(double)) || length > SIZE_MAX / n
      || length * n > SIZE_MAX / sizeof(size_t)) return 0;
  a = (double *)malloc(2 * n * sizeof(*a));
  back = (size_t *)malloc(length * n * sizeof(*back));
  order = (size_t *)malloc(n * sizeof(*order));
  if (!a || !back || !order) { free(a); free(back); free(order); return 0; }
  b = a + n;
  for (size_t t = 0; t < length; ++t) {
    for (size_t j = 0; j < n; ++j) {
      double score = t ? -INFINITY : mulib_viterbi_log_(m->initial[j]);
      size_t previous = 0;
      if (t) for (size_t i = 0; i < n; ++i) {
        double candidate = a[i] + mulib_viterbi_log_(m->transition[i * n + j]);
        if (candidate > score) { score = candidate; previous = i; }
      }
      b[j] = score + mulib_viterbi_log_(m->emission[j * m->symbols + obs[t]]);
      back[t * n + j] = previous;
    }
    for (size_t j = 0; j < n; ++j) {
      size_t k = j;
      while (k && b[order[k - 1]] < b[j]) { order[k] = order[k - 1]; --k; }
      order[k] = j;
    }
    for (size_t k = width; k < n; ++k) b[order[k]] = -INFINITY;
    for (size_t j = 0; j < n; ++j) a[j] = b[j];
  }
  best = a[0];
  for (size_t j = 1; j < n; ++j) if (a[j] > best) { best = a[j]; state = j; }
  if (!isfinite(best)) { free(a); free(back); free(order); return 0; }
  for (size_t t = length; t-- > 0;) { path[t] = state; state = back[t * n + state]; }
  *out = best; free(a); free(back); free(order); return 1;
}
int mulib_viterbi_decode(const mulib_hidden_markov_chain_t *m, const size_t *obs,
                        size_t length, size_t *path, double *out)
{ return mulib_viterbi_decode_beam(m, obs, length, m ? m->states : 0, path, out); }
#ifdef __cplusplus
}
#endif
#endif
