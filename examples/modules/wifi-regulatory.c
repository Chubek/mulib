#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NETLINK_GENL
#define MULIB_IMPL_NL80211
#define MULIB_IMPL_WIFI_REGULATORY
#include "mulib-wifi-regulatory.h"
#include <stdio.h>
int
main (void)
{
  printf ("regulatory query status=%d\n",
          mulib_wifi_regulatory_query (NULL, NULL));
  return 0;
}
