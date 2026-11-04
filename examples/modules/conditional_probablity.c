#define MULIB_IMPL_CONDITIONAL_PROBABLITY
#include "mulib-conditional_probablity.h"
#include <assert.h>
int main(void) {
  assert(mulib_conditional_probability(0.25, 0.5) == 0.5);
  assert(mulib_conditional_bayes(0.5, 0.5, 0.5) == 0.5);
  return 0;
}
