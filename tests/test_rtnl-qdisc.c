/* Generated, standalone MuLIB add-on symbol and API test: rtnl-qdisc */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_RTNL
#define MULIB_IMPL_RTNL_QDISC
#include "mulib-rtnl-qdisc.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_rtnl_qdisc_dump;
    assert (p);
  }
  puts ("PASS rtnl-qdisc");
  return 0;
}
