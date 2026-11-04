#define MULIB_IMPL_PROBABILITY
#define MULIB_IMPL_MARKOV_CHAIN
#define MULIB_IMPL_HIDDEN_MARKOV_CHAIN
#define MULIB_IMPL_VITERBI_DECODING
#include "mulib-viterbi_decoding.h"
#include <assert.h>
int main(void) {
  double p[] = {1}, e[] = {0.5, 0.5}, logp; size_t obs[] = {0, 1}, path[2];
  mulib_hidden_markov_chain_t model = {1, 2, p, p, e};
  assert(mulib_viterbi_decode(&model, obs, 2, path, &logp));
  assert(path[0] == 0 && path[1] == 0); return 0;
}
