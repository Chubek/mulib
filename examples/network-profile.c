#include <stdint.h>
#include <stddef.h>
#define MULIB_IMPL_NET_PROFILE
#define MULIB_IMPL_NET_RECONCILE
#define MULIB_IMPL_NET_STATE
#include "mulib-net-reconcile.h"
#include <stdio.h>
#include <string.h>
int
main (void)
{
  const char *txt = "interface=lo\nenabled=1\n";
  mulib_net_profile_t p;
  if (mulib_net_profile_parse (txt, strlen (txt), &p))
    return 1;
  mulib_net_reconcile_result_t r;
  int err = mulib_net_reconcile_check (&p, &r);
  printf ("loopback exists: %d, operational: %d, differs: %d, err=%d\n",
          r.interface_present, r.oper_up, r.differs, err);
  return !!err;
}
