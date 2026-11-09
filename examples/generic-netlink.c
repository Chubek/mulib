#include <stdint.h>
#include <stddef.h>
#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NETLINK_GENL
#include "mulib-netlink-genl.h"
#include <stdio.h>
#include <linux/netlink.h>
int
main (void)
{
  mulib_netlink_socket_t s = MULIB_NETLINK_INIT;
  int r = mulib_netlink_open (&s, NETLINK_GENERIC, 0);
  if (r)
    {
      printf ("open error %d\n", r);
      return 1;
    }
  uint16_t id = 0;
  r = mulib_netlink_genl_resolve (&s, "nl80211", &id);
  printf ("nl80211 family: %u (error %d)\n", id, r);
  mulib_netlink_close (&s);
  return 0;
}
