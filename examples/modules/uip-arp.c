#define MULIB_IMPL_UIP_ARP
#include "mulib-uip-arp.h"
#include <assert.h>
int
main (void)
{
  mulib_uip_arp_packet_t arp = { 0 }, decoded;
  arp.operation = 1;
  arp.sender_mac[0] = 2;
  arp.sender_ip[0] = 10;
  uint8_t bytes[28];
  assert (mulib_uip_arp_pack (bytes, sizeof bytes, &arp) == 28);
  assert (!mulib_uip_arp_parse (bytes, sizeof bytes, &decoded));
  assert (decoded.operation == 1);
  return 0;
}
