/* Generated, standalone MuLIB add-on symbol and API test: nl80211 */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NETLINK_GENL
#define MULIB_IMPL_NL80211
#include "mulib-nl80211.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_nl80211_command;
    assert (p);
  }
  puts ("PASS nl80211");
  return 0;
}
