#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_WIFI_MONITOR
#include "mulib-wifi-monitor.h"
#include <stdio.h>
int
main (void)
{
  mulib_netlink_socket_t s = MULIB_NETLINK_INIT;
  printf ("link subscription status=%d\n", mulib_wifi_monitor_open (&s));
  mulib_netlink_close (&s);
  return 0;
}
