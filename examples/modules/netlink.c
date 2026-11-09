/* Read-only Netlink socket ownership example. */
#define MULIB_IMPL_NETLINK
#include "mulib-netlink.h"
#include <assert.h>
#include <linux/netlink.h>
int main(void) {
  mulib_netlink_socket_t s = MULIB_NETLINK_INIT;
  assert(!mulib_netlink_open(&s, NETLINK_ROUTE, 0));
  assert(!mulib_netlink_close(&s));
  return 0;
}
