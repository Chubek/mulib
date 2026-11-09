/* Generated, standalone MuLIB add-on symbol and API test: net-cap */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NET_CAP
#include "mulib-net-cap.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_net_cap_effective;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_net_cap_has;
    assert (p);
  }

  uint64_t v = 0;
  assert (!mulib_net_cap_effective (&v));
  int yes = 0;
  assert (!mulib_net_cap_has (0, &yes));
  puts ("PASS net-cap");
  return 0;
}
