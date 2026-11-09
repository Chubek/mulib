#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NETLINK_GENL
#define MULIB_IMPL_NL80211
#define MULIB_IMPL_WIFI_AP
#include "mulib-wifi-ap.h"
#include <assert.h>
int
main (void)
{
  assert (mulib_wifi_ap_query (0, NULL, NULL) == -EINVAL);
  return 0;
}
