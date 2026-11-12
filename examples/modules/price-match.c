/* Complete, offline price-match example. */
#define MULIB_IMPL_PRICE_MATCH
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#include "mulib-price-match.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  double score;
  assert (mulib_price_match_gtin ("4006381333931"));
  assert (mulib_price_match_score ("Widget-2", "widget 2", &score)
          && score == 1);
  return 0;
}
