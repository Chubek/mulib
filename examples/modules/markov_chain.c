#define MULIB_IMPL_PROBABILITY
#define MULIB_IMPL_MARKOV_CHAIN
#include "mulib-markov_chain.h"
#include <assert.h>
#include <math.h>
int main(void) {
  double matrix[] = {0.7, 0.3, 0.4, 0.6}, p[] = {1, 0}, out[2];
  mulib_markov_chain_t chain = {2, matrix};
  assert(mulib_markov_chain_step(&chain, p, out) && fabs(out[0] - 0.7) < 1e-12);
  assert(mulib_markov_chain_sample(&chain, 0, 0.8) == 1); return 0;
}
