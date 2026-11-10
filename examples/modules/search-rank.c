#define MULIB_IMPL_SEARCH_RANK
#include "mulib-search-rank.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int
main (void)
{
  assert (mulib_search_rank_count ("hello HELLO", "hello") == 2);
  assert (mulib_search_rank_words ("cat cats CAT", "cat") == 2);
  assert (mulib_search_rank_fields ("cat", "cat cat", "cat", 2, 0.5) == 3);
  return 0;
}
