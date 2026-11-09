#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NETLINK_GENL
#define MULIB_IMPL_NL80211
#define MULIB_IMPL_WIFI_STATION
#include "mulib-wifi-station.h"
#include <assert.h>
int
main (void)
{
  assert (mulib_wifi_station_query (0, NULL, NULL) == -EINVAL);
  return 0;
}
