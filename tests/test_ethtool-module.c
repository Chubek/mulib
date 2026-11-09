/* Generated, standalone MuLIB add-on symbol and API test: ethtool-module */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_ETHTOOL_MODULE
#include "mulib-ethtool-module.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_ethtool_module_info;
    assert (p);
  }
  puts ("PASS ethtool-module");
  return 0;
}
