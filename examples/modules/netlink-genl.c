#define MULIB_IMPL_NETLINK
#define MULIB_IMPL_NETLINK_ATTR
#define MULIB_IMPL_NETLINK_GENL
#include "mulib-netlink-genl.h"
#include <stdio.h>
int
main (void)
{
  mulib_netlink_socket_t s = MULIB_NETLINK_INIT;
  uint16_t id = 0;
  int r = mulib_netlink_open (&s, NETLINK_GENERIC, 0);
  if (!r)
    r = mulib_netlink_genl_resolve (&s, "nlctrl", &id);
  printf ("nlctrl: status=%d id=%u\n", r, (unsigned)id);
  mulib_netlink_close (&s);
  return 0;
}
