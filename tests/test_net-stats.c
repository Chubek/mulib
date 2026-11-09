/* Generated, standalone MuLIB add-on symbol and API test: net-stats */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NET_STATS
#include "mulib-net-stats.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_net_stats_read;
    assert (p);
  }

  mulib_net_stats_t s;
  assert (!mulib_net_stats_read ("lo", &s));
  puts ("PASS net-stats");
  return 0;
}
