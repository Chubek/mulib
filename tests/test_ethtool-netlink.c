/* Generated, standalone MuLIB add-on symbol and API test: ethtool-netlink */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_ETHTOOL_NETLINK
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NETLINK_GENL
#include "mulib-ethtool-netlink.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_ethtool_netlink_family;
    assert (p);
  }
  puts ("PASS ethtool-netlink");
  return 0;
}
