#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NETLINK_GENL
#define MULIB_IMPL_NL80211
#define MULIB_IMPL_WIFI_SCAN
#include "mulib-wifi-scan.h"
#include <assert.h>
/* A real query requires a caller-selected wireless interface index. */
int
main (void)
{
  assert (mulib_wifi_scan_query (0, NULL, NULL) == -EINVAL);
  return 0;
}
