#define MULIB_IMPL_EUCLIDEAN_DISTANCE
#include "mulib-euclidean_distance.h"
#include <assert.h>
int main(void) {
  double a[] = {0, 0}, b[] = {3, 4};
  assert(mulib_euclidean_distance(a, b, 2) == 5);
  return 0;
}
