/* Generated, standalone MuLIB add-on symbol and API test: dhcp4-client */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_DHCP4_CLIENT
#include "mulib-dhcp4-client.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_dhcp4_client_discover;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_dhcp4_client_type;
    assert (p);
  }

  uint8_t b[244], mac[6] = { 1, 2, 3, 4, 5, 6 }, typ = 0;
  size_t n = 0;
  assert (!mulib_dhcp4_client_discover (b, sizeof b, mac, 0x11223344, &n));
  assert (n == 244);
  assert (!mulib_dhcp4_client_type (b, n, &typ) && typ == 1);
  puts ("PASS dhcp4-client");
  return 0;
}
