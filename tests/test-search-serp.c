#define MULIB_IMPL_SEARCH_SERP
#include "mulib-search-serp.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  char b[80];
  assert (
      mulib_search_serp_href ("<a href=\"https://a\">A</a>", 0, b, sizeof b));
  assert (!strcmp (b, "https://a"));
  return 0;
}
