/* Complete, offline price-causal example. */
#define MULIB_IMPL_PRICE_CAUSAL
#include "mulib-price-causal.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  double before[] = { 10, 20 }, treated[] = { 15, 25 }, control[] = { 11, 21 };
  mulib_price_causal_result_t result;
  assert (mulib_price_causal_did (before, treated, 2, before, control, 2,
                                  &result));
  assert (result.effect == 4);
  return 0;
}
