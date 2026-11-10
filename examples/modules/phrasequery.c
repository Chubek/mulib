#define MULIB_IMPL_POSITIONS
#define MULIB_IMPL_PHRASEQUERY
#include "mulib-phrasequery.h"
#include <assert.h>
int
main (void)
{
  mulib_positions_t phrase[2] = { { 0 } };
  assert (mulib_positions_add (&phrase[0], 4) && mulib_positions_add (&phrase[1], 5));
  assert (mulib_phrasequery_match (phrase, 2));
  mulib_positions_free (&phrase[0]);
  mulib_positions_free (&phrase[1]);
  return 0;
}
