#define MULIB_IMPL_TOPK
#include "mulib-topk.h"
#include <assert.h>
int
main (void)
{
  mulib_topk_entry_t scores[] = { { 9, 1 }, { 3, 2 }, { 2, 2 } }, best[2];
  assert (mulib_topk_select (scores, 3, best, 2) == 2 && best[0].id == 2
          && best[1].id == 3);
  return 0;
}
