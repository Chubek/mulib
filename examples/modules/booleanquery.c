#define MULIB_IMPL_POSTINGS
#define MULIB_IMPL_BOOLEANQUERY
#include "mulib-booleanquery.h"
#include <assert.h>
int
main (void)
{
  mulib_postings_t a = { 0 }, b = { 0 }, both = { 0 };
  assert (mulib_postings_put (&a, 10, 2) && mulib_postings_put (&a, 20, 1));
  assert (mulib_postings_put (&b, 20, 3));
  assert (mulib_booleanquery_apply (&a, &b, MULIB_BOOLEAN_AND, &both));
  assert (both.count == 1 && both.items[0].document == 20
          && both.items[0].frequency == 3);
  mulib_postings_free (&a);
  mulib_postings_free (&b);
  mulib_postings_free (&both);
  return 0;
}
