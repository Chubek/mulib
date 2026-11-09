#define MULIB_IMPL_UIP_ARP_CACHE
#include "mulib-uip-arp-cache.h"
#include <assert.h>
int
main (void)
{
  mulib_uip_arp_cache_t cache = { 0 };
  uint8_t ip[4] = { 10, 0, 0, 1 }, mac[6] = { 2 }, found[6];
  assert (!mulib_uip_arp_cache_store (&cache, ip, mac, 100, 10));
  assert (!mulib_uip_arp_cache_find (&cache, ip, 101, found));
  assert (mulib_uip_arp_cache_find (&cache, ip, 110, found) == -ENOENT);
  return 0;
}
