/* Generated, standalone MuLIB add-on symbol and API test: rtnl-route */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_RTNL
#define MULIB_IMPL_RTNL_ROUTE
#include "mulib-rtnl-route.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_rtnl_route_dump;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_rtnl_route_change;
    assert (p);
  }

  assert (mulib_rtnl_route_change (4, NULL, 1, NULL, 0, 0, 1) < 0);
  puts ("PASS rtnl-route");
  return 0;
}
