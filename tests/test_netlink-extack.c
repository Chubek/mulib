/* Generated, standalone MuLIB add-on symbol and API test: netlink-extack */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_EXTACK
#include "mulib-netlink-extack.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_netlink_extack_parse;
    assert (p);
  }
  puts ("PASS netlink-extack");
  return 0;
}
