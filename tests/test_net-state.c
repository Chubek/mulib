/* Generated, standalone MuLIB add-on symbol and API test: net-state */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NET_STATE
#include "mulib-net-state.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_net_state_read;
    assert (p);
  }

  mulib_net_state_t s;
  assert (!mulib_net_state_read ("lo", &s));
  assert (s.mtu > 0 && s.index > 0);
  puts ("PASS net-state");
  return 0;
}
