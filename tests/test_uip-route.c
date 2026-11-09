/* Generated, standalone MuLIB add-on symbol and API test: uip-route */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_UIP_ROUTE
#include "mulib-uip-route.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_route_add;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_route_find;
    assert (p);
  }

  mulib_uip_routes_t t = { 0 };
  uint8_t net[4] = { 10, 0, 0, 0 }, target[4] = { 10, 1, 3, 4 };
  mulib_uip_route_entry_t e;
  assert (!mulib_uip_route_add (&t, net, 8, NULL, 2));
  assert (!mulib_uip_route_find (&t, target, &e));
  assert (e.interface_id == 2);
  puts ("PASS uip-route");
  return 0;
}
