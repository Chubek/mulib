#define MULIB_IMPL_PROBABILITY
#include "mulib-probability.h"
#include <assert.h>
int main(void) {
  double p[] = {2, 6};
  assert(mulib_probability_normalize(p, 2));
  assert(p[0] == 0.25 && mulib_probability_valid(p, 2, 1e-12));
  return 0;
}
