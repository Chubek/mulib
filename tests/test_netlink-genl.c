/* Generated, standalone MuLIB add-on symbol and API test: netlink-genl */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NETLINK_GENL
#include "mulib-netlink-genl.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_netlink_genl_resolve;
    assert (p);
  }
  puts ("PASS netlink-genl");
  return 0;
}
