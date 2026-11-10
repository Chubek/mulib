#define MULIB_IMPL_HYBRIDSEARCH
#include "mulib-hybridsearch.h"
#include <assert.h>
int
main (void)
{
  uint64_t lexical[] = { 10, 20 }, vector[] = { 20, 30 };
  mulib_hybridsearch_hit_t hits[3];
  assert (mulib_hybridsearch_rrf (lexical, 2, vector, 2, 60, hits, 3) == 3
          && hits[0].id == 20);
  return 0;
}
