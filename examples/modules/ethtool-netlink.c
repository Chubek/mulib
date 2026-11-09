#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NETLINK_GENL
#define MULIB_IMPL_ETHTOOL_NETLINK
#include "mulib-ethtool-netlink.h"
#include <stdio.h>
int
main (void)
{
  uint16_t id = 0;
  int r = mulib_ethtool_netlink_family (&id);
  printf ("ethtool: status=%d id=%u\n", r, (unsigned)id);
  return 0;
}
