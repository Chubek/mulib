/* Generated, standalone MuLIB add-on symbol and API test: rtnl-bridge */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_RTNL
#define MULIB_IMPL_RTNL_BRIDGE
#define MULIB_IMPL_RTNL_LINK
#include "mulib-rtnl-bridge.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_rtnl_bridge_dump;
    assert (p);
  }
  puts ("PASS rtnl-bridge");
  return 0;
}
