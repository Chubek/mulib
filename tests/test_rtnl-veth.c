/* Generated, standalone MuLIB add-on symbol and API test: rtnl-veth */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_RTNL
#define MULIB_IMPL_RTNL_LINK
#define MULIB_IMPL_RTNL_VETH
#include "mulib-rtnl-veth.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_rtnl_veth_dump;
    assert (p);
  }
  puts ("PASS rtnl-veth");
  return 0;
}
