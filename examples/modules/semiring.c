#define MULIB_IMPL_SEMIRING
#include "mulib-semiring.h"
#include <assert.h>
int
main (void)
{
  /* Shortest paths: concatenate with +, choose with min. */
  mulib_semiring_kind_t kind = MULIB_SEMIRING_MIN_PLUS;
  double via = mulib_semiring_mul (kind, 3, 4);
  assert (mulib_semiring_add (kind, 10, via) == 7);
  assert (mulib_semiring_mul (kind, mulib_semiring_zero (kind), 3) == INFINITY);
  return 0;
}
