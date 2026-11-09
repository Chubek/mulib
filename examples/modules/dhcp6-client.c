#define MULIB_IMPL_DHCP6_CLIENT
#include "mulib-dhcp6-client.h"
#include <assert.h>
int
main (void)
{
  uint8_t packet[4];
  size_t n = 0;
  assert (!mulib_dhcp6_client_solicit (packet, sizeof packet, 42, &n));
  assert (n == 4 && packet[0] == 1);
  return 0;
}
