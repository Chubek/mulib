#define MULIB_IMPL_PROBABILITY
#define MULIB_IMPL_BAYESIAN_METHODS
#include "mulib-bayesian_methods.h"
#include <assert.h>
#include <math.h>
int main(void) {
  double p[] = {0.5, 0.5}, ll[] = {log(0.8), log(0.2)}, out[2];
  assert(mulib_bayesian_update(p, ll, 2, out, NULL));
  assert(fabs(out[0] - 0.8) < 1e-12);
  return 0;
}
