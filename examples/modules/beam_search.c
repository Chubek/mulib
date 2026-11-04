#define MULIB_IMPL_PROBABILITY
#define MULIB_IMPL_MARKOV_CHAIN
#define MULIB_IMPL_HIDDEN_MARKOV_CHAIN
#define MULIB_IMPL_VITERBI_DECODING
#define MULIB_IMPL_BEAM_SEARCH
#include "mulib-beam_search.h"
#include <assert.h>
int main(void) {
  double p[] = {1}, logp; size_t obs[] = {0}, path[1];
  mulib_hidden_markov_chain_t model = {1, 1, p, p, p};
  assert(mulib_beam_search(&model, obs, 1, 1, path, &logp) && path[0] == 0); return 0;
}
