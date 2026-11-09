/* Generated, standalone MuLIB add-on symbol and API test: rtnl-rule */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_RTNL
#define MULIB_IMPL_RTNL_RULE
#include "mulib-rtnl-rule.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_rtnl_rule_dump;
    assert (p);
  }
  puts ("PASS rtnl-rule");
  return 0;
}
