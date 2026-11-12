/* Complete, offline webextract-reconcile example. */
#define MULIB_IMPL_WEBEXTRACT_RECONCILE
#include "mulib-webextract-reconcile.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_webextract_candidate_t c[]
      = { { 12, .9, 1, 1 }, { 12.1, .8, 0, 1 }, { 99, .1, 0, 0 } };
  double price;
  size_t selected;
  assert (mulib_webextract_reconcile (c, 3, .02, &price, &selected)
          && selected == 0);
  return 0;
}
