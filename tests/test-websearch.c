#define MULIB_IMPL_WEBSEARCH
#include "mulib-websearch.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  assert (
      !strcmp (mulib_websearch_provider_name (MULIB_WEBSEARCH_BING), "bing"));
  return 0;
}
