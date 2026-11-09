#define MULIB_IMPL_NET_PROFILE
#define MULIB_IMPL_NET_STATE
#define MULIB_IMPL_NET_RECONCILE
#include "mulib-net-reconcile.h"
#include <stdio.h>
int
main (void)
{
  mulib_net_profile_t profile = { "lo", "", "", 1 };
  mulib_net_reconcile_result_t result;
  int r = mulib_net_reconcile_check (&profile, &result);
  if (!r)
    printf ("present=%d differs=%d (read-only)\n", result.interface_present,
            result.differs);
  return !!r;
}
