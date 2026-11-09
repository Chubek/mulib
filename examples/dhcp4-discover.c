#include <stdint.h>
#include <stddef.h>
#define MULIB_IMPL_DHCP4_CLIENT
#include "mulib-dhcp4-client.h"
#include <stdio.h>
int
main (void)
{
  uint8_t mac[6] = { 2, 0, 0, 0, 0, 1 }, buf[512];
  size_t n = 0;
  if (mulib_dhcp4_client_discover (buf, sizeof buf, mac, 0x12345678, &n))
    return 1;
  printf ("DHCPv4 DISCOVER encoded: %zu bytes (not sent)\n", n);
  return 0;
}
