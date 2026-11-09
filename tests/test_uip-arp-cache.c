/* Generated, standalone MuLIB add-on symbol and API test: uip-arp-cache */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_UIP_ARP_CACHE
#include "mulib-uip-arp-cache.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_arp_cache_store;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_arp_cache_find;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_arp_cache_expire;
    assert (p);
  }

  mulib_uip_arp_cache_t c = { 0 };
  uint8_t ip[4] = { 1 }, mac[6] = { 3 }, out[6] = { 0 };
  assert (!mulib_uip_arp_cache_store (&c, ip, mac, 100, 10));
  assert (!mulib_uip_arp_cache_find (&c, ip, 105, out));
  assert (out[0] == 3);
  assert (mulib_uip_arp_cache_find (&c, ip, 110, out) == -ENOENT);
  puts ("PASS uip-arp-cache");
  return 0;
}
