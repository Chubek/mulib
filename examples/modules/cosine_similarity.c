#define MULIB_IMPL_COSINE_SIMILARITY
#include "mulib-cosine_similarity.h"
#include <assert.h>
#include <math.h>
int main(void) {
  double a[] = {1, 2}, b[] = {2, 4};
  assert(fabs(mulib_cosine_similarity(a, b, 2) - 1) < 1e-12);
  return 0;
}
