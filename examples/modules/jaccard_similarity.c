#define MULIB_IMPL_JACCARD_SIMILARITY
#include "mulib-jaccard_similarity.h"
#include <assert.h>
#include <math.h>
int main(void) {
  uint64_t a[] = {1, 1, 2}, b[] = {2, 3};
  assert(fabs(mulib_jaccard_similarity(a, 3, b, 2) - 1.0 / 3) < 1e-12);
  return 0;
}
