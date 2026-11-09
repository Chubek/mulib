/* Generated, standalone MuLIB add-on symbol and API test: rtnl-neighbor */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_RTNL
#define MULIB_IMPL_RTNL_NEIGHBOR
#include "mulib-rtnl-neighbor.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_rtnl_neighbor_dump;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_rtnl_neighbor_change;
    assert (p);
  }

  assert (mulib_rtnl_neighbor_change (0, 4, NULL, NULL, 1) < 0);
  puts ("PASS rtnl-neighbor");
  return 0;
}
