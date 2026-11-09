#define MULIB_IMPL_UIP_ROUTE
#include "mulib-uip-route.h"
#include <assert.h>
int
main (void)
{
  mulib_uip_routes_t routes = { 0 };
  uint8_t prefix[4] = { 10 }, target[4] = { 10, 1, 2, 3 };
  assert (!mulib_uip_route_add (&routes, prefix, 8, NULL, 1));
  mulib_uip_route_entry_t found;
  assert (!mulib_uip_route_find (&routes, target, &found));
  assert (found.interface_id == 1);
  return 0;
}
