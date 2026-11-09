/* Serialization only: no DHCP packet is sent. */
#define MULIB_IMPL_DHCP4_CLIENT
#include "mulib-dhcp4-client.h"
#include <assert.h>
int
main (void)
{
  uint8_t packet[244], mac[6] = { 2, 0, 0, 0, 0, 1 }, type;
  size_t n = 0;
  assert (!mulib_dhcp4_client_discover (packet, sizeof packet, mac, 42, &n));
  assert (!mulib_dhcp4_client_type (packet, n, &type));
  assert (type == 1);
  return 0;
}
