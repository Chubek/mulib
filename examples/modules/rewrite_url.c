#define MULIB_IMPL_REWRITE_URL
#define MULIB_IMPL_SECURITY
#include "mulib-rewrite_url.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int
main (void)
{
  char *target = NULL;
  assert (mulib_rewrite_url ("/api/item?q=1", "/api", "/v2", &target) == 1);
  assert (!strcmp (target, "/v2/item?q=1"));
  free (target);
  return 0;
}
