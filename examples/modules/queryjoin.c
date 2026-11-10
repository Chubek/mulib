#define MULIB_IMPL_QUERYJOIN
#include "mulib-queryjoin.h"
#include <assert.h>
int
main (void)
{
  mulib_queryjoin_row_t a[] = { { 1, 10 }, { 2, 11 } }, b[] = { { 2, 20 }, { 2, 21 } };
  mulib_queryjoin_hit_t pairs[2];
  assert (mulib_queryjoin_merge (a, 2, b, 2, pairs, 2) == 2);
  assert (pairs[0].left_row == 11 && pairs[0].right_row == 20
          && pairs[1].right_row == 21);
  return 0;
}
