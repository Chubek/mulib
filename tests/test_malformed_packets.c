/* Bounded malformed-input checks for core userspace packet parsing. */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#define MULIB_IMPL_UIP_ARP
#define MULIB_IMPL_UIP_ARP_CACHE
#define MULIB_IMPL_UIP_CHECKSUM
#define MULIB_IMPL_UIP_ETHERNET
#define MULIB_IMPL_UIP_ICMP
#define MULIB_IMPL_UIP_IPV4
#define MULIB_IMPL_UIP_UDP
#define MULIB_IMPL_USTACK
#include "mulib-ustack.h"
int
main (void)
{
  uint8_t invalid[128] = { 0 };
  const uint8_t src[4] = { 10, 0, 0, 1 }, dst[4] = { 10, 0, 0, 2 };
  for (size_t n = 0; n <= sizeof invalid; n++)
    {
      mulib_uip_ipv4_packet_t ip;
      mulib_uip_udp_datagram_t udp;
      mulib_uip_arp_packet_t arp;
      assert (mulib_uip_ipv4_parse (invalid, n, &ip) < 0);
      assert (mulib_uip_udp_parse (src, dst, invalid, n, &udp) < 0);
      assert (mulib_uip_arp_parse (invalid, n, &arp) < 0);
    }
  puts ("PASS malformed input: 129 lengths each for IPv4/UDP/ARP");
  return 0;
}
