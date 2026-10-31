#include "../container-stack-common.h"
int main (void)
{
  mulib_container_netdev a, b;
  unsigned char packet[20] = { 0x45, 0, 0, 20 }, received[20];
  size_t length;
  assert (!mulib_container_netdev_init (&a, "a", MULIB_CONTAINER_NETDEV_IP, 1500, 4, 4096));
  assert (!mulib_container_netdev_init (&b, "b", MULIB_CONTAINER_NETDEV_IP, 1500, 4, 4096));
  assert (!mulib_container_netdev_attach (&a, mulib_container_netdev_peer, &b, NULL));
  assert (!mulib_container_netdev_link (&a, 1) && !mulib_container_netdev_link (&b, 1));
  assert (!mulib_container_netdev_send (&a, packet, sizeof (packet)));
  assert (!mulib_container_netdev_receive (&b, received, sizeof (received), &length));
  assert (length == sizeof (packet) && !memcmp (packet, received, length));
  mulib_container_netdev_free (&a); mulib_container_netdev_free (&b);
  return 0;
}
