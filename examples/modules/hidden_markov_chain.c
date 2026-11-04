#define MULIB_IMPL_PROBABILITY
#define MULIB_IMPL_MARKOV_CHAIN
#define MULIB_IMPL_HIDDEN_MARKOV_CHAIN
#include "mulib-hidden_markov_chain.h"
#include <assert.h>
#include <math.h>
int main(void) {
  double initial[] = {1}, transition[] = {1}, emission[] = {0.25, 0.75}, logp;
  size_t observations[] = {1, 1};
  mulib_hidden_markov_chain_t model = {1, 2, initial, transition, emission};
  assert(mulib_hidden_markov_chain_forward(&model, observations, 2, &logp));
  assert(fabs(exp(logp) - 0.5625) < 1e-12); return 0;
}
