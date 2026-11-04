#define MULIB_IMPL_HAMMING_DISTANCE
#include "mulib-hamming_distance.h"
#include <assert.h>
int main(void) {
  assert(mulib_hamming_distance("cat", "car", 3) == 1);
  assert(mulib_hamming_bits("A", "C", 1) == 1);
  return 0;
}
