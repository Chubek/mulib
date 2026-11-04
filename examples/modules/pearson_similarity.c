#define MULIB_IMPL_PEARSON_SIMILARITY
#include "mulib-pearson_similarity.h"
#include <assert.h>
#include <math.h>
int main(void) {
  double a[] = {1, 2, 3}, b[] = {9, 7, 5};
  assert(fabs(mulib_pearson_similarity(a, b, 3) + 1) < 1e-12);
  return 0;
}
