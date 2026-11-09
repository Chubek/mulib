/* Generated, standalone MuLIB add-on symbol and API test: rtnl-nexthop */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_RTNL
#define MULIB_IMPL_RTNL_NEXTHOP
#include "mulib-rtnl-nexthop.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_rtnl_nexthop_dump;
    assert (p);
  }
  puts ("PASS rtnl-nexthop");
  return 0;
}
