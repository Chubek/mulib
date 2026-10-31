#include "../container-stack-common.h"
int main (void)
{
  mulib_container_network network;
  mulib_container_network_route route;
  mulib_container_netdev device;
  unsigned char address[4] = { 10, 0, 0, 5 };
  assert (!mulib_container_netdev_init (&device, "net0", MULIB_CONTAINER_NETDEV_IP, 1500, 4, 4096));
  assert (!mulib_container_netdev_link (&device, 1));
  assert (!mulib_container_network_init (&network, 4, 16, 0));
  memset (&route, 0, sizeof (route)); route.family = 4; route.prefix = 24;
  route.network[0] = 10; route.device = &device;
  assert (!mulib_container_network_add (&network, &route));
  assert (mulib_container_network_lookup (&network, 4, address)->device == &device);
  mulib_container_network_free (&network); mulib_container_netdev_free (&device);
  return 0;
}
