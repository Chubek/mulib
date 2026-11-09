/* Generated, standalone MuLIB add-on symbol and API test: rtnl-tunnel */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_RTNL
#define MULIB_IMPL_RTNL_LINK
#define MULIB_IMPL_RTNL_TUNNEL
#include "mulib-rtnl-tunnel.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_rtnl_tunnel_dump;
    assert (p);
  }
  puts ("PASS rtnl-tunnel");
  return 0;
}
