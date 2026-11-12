/* Complete, offline price-confidence example. */
#define MULIB_IMPL_PRICE_CONFIDENCE
#include "mulib-price-confidence.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  double scores[] = { .8, .6 }, weights[] = { 3, 1 }, confidence;
  assert (mulib_price_confidence_combine (scores, weights, 2, &confidence));
  assert (fabs (confidence - .75) < 1e-12);
  return 0;
}
