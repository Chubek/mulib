/** @file mulib-beam_search.h
 * @brief State-pruned beam search over discrete HMM trellises.
 * Define MULIB_IMPL_BEAM_SEARCH and VITERBI_DECODING plus its dependencies.
 * This retains states, not multiple histories per state; it is not an N-best API.
 */
#ifndef MULIB_BEAM_SEARCH_H
#define MULIB_BEAM_SEARCH_H
#include "mulib-viterbi_decoding.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Approximate best path; width>=states gives Viterbi. Return 0 for invalid,
 * OOM or no surviving complete path, preserving outputs. No optimality promise
 * when width<states; ties and ownership follow mulib_viterbi_decode. */
int mulib_beam_search(const mulib_hidden_markov_chain_t *model, const size_t *observations,
                      size_t length, size_t width, size_t *path, double *log_probability);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BEAM_SEARCH) && !defined(MULIB_BEAM_SEARCH_IMPLEMENTED)
#define MULIB_BEAM_SEARCH_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_beam_search(const mulib_hidden_markov_chain_t *m, const size_t *obs,
                      size_t length, size_t width, size_t *path, double *out)
{ return mulib_viterbi_decode_beam(m, obs, length, width, path, out); }
#ifdef __cplusplus
}
#endif
#endif
