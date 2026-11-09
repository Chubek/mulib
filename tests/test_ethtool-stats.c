/* Generated, standalone MuLIB add-on symbol and API test: ethtool-stats */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_ETHTOOL_STATS
#include "mulib-ethtool-stats.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_ethtool_stats_sysfs;
    assert (p);
  }
  puts ("PASS ethtool-stats");
  return 0;
}
