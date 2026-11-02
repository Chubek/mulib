#define MULIB_IMPL_VHPST
#define MULIB_IMPL_SECURITY
#include "mulib-vhpst.h"
#include <assert.h>
int
main (void)
{
  const mulib_vhpst_t hosts[]
      = { { "example.org", NULL }, { "api.example.org", NULL } };
  assert (mulib_vhpst_find (hosts, 2, "API.EXAMPLE.ORG") == 1);
  assert (mulib_vhpst_find (hosts, 2, "unknown.org") == SIZE_MAX);
  return 0;
}
