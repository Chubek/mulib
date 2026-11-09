/* Generated, standalone MuLIB add-on symbol and API test: dhcp6-client */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_DHCP6_CLIENT
#include "mulib-dhcp6-client.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_dhcp6_client_solicit;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_dhcp6_client_option;
    assert (p);
  }

  uint8_t b[4];
  size_t n = 0;
  assert (!mulib_dhcp6_client_solicit (b, 4, 0x112233, &n));
  assert (n == 4 && b[0] == 1);
  puts ("PASS dhcp6-client");
  return 0;
}
