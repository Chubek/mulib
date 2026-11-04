#define MULIB_IMPL_KMM
#include "mulib-kmm.h"
#include <assert.h>
#include <math.h>
int main(void) {
  double kernel[] = {1, 0, 0, 1}, kappa[] = {1.5, 0.5}, weights[2];
  assert(mulib_kmm_fit(kernel, kappa, 2, 2, 100, 1e-10, weights) == 1);
  assert(fabs(weights[0] - 1.5) < 1e-9); return 0;
}
