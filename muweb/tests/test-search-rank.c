#define MULIB_IMPL_SEARCH_RANK
#include "mulib-search-rank.h"
#include <assert.h>
#include <string.h>
int main(void) {
assert(mulib_search_rank_count("hello HELLO","hello")==2);
return 0;
}
