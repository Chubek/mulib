/* Generated, standalone MuLIB add-on symbol and API test: ethtool-features */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_ETHTOOL_FEATURES
#include "mulib-ethtool-features.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_ethtool_features_get;
    assert (p);
  }
  puts ("PASS ethtool-features");
  return 0;
}
