/* Generated, standalone MuLIB add-on symbol and API test: netlink */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NETLINK
#include "mulib-netlink.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_netlink_open;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_netlink_close;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_netlink_send;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_netlink_receive;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_netlink_transact;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_netlink_foreach;
    assert (p);
  }

  mulib_netlink_socket_t s = MULIB_NETLINK_INIT;
  assert (mulib_netlink_open (NULL, 0, 0) < 0);
  int r = mulib_netlink_open (&s, NETLINK_ROUTE, 0);
  assert (r == 0);
  assert (s.fd >= 0);
  assert (!mulib_netlink_close (&s));
  puts ("PASS netlink");
  return 0;
}
