#define MULIB_IMPL_NET_STATE
#include "mulib-net-state.h"
#include <stdio.h>
int
main (void)
{
  mulib_net_state_t s;
  int r = mulib_net_state_read ("lo", &s);
  if (!r)
    printf ("loopback index=%u MTU=%u oper-up=%d\n", s.index, s.mtu,
            s.oper_up);
  return !!r;
}
