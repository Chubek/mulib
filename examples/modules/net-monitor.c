#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NET_MONITOR
#include "mulib-net-monitor.h"
#include <stdio.h>
int
main (void)
{
  mulib_netlink_socket_t s = MULIB_NETLINK_INIT;
  printf ("network subscription status=%d\n", mulib_net_monitor_open (&s));
  mulib_netlink_close (&s);
  return 0;
}
