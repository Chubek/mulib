#define MULIB_IMPL_POSTINGS
#include "mulib-postings.h"
#include <assert.h>
int
main (void)
{
  mulib_postings_t p = { 0 };
  uint32_t frequency;
  assert (mulib_postings_put (&p, 42, 3) && mulib_postings_put (&p, 7, 1));
  assert (mulib_postings_find (&p, 42, &frequency) && frequency == 3);
  assert (mulib_postings_remove (&p, 7) && p.count == 1);
  mulib_postings_free (&p);
  return 0;
}
