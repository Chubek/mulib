/* Generated, standalone MuLIB add-on symbol and API test: uip-arp */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_UIP_ARP
#include "mulib-uip-arp.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_arp_parse;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_arp_pack;
    assert (p);
  }

  mulib_uip_arp_packet_t a = { 0 }, b = { 0 };
  a.operation = 1;
  a.sender_ip[0] = 1;
  uint8_t f[28];
  assert (mulib_uip_arp_pack (f, sizeof f, &a) == 28);
  assert (!mulib_uip_arp_parse (f, sizeof f, &b));
  assert (b.sender_ip[0] == 1);
  puts ("PASS uip-arp");
  return 0;
}
