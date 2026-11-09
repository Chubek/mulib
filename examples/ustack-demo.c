/* Fully offline Ethernet/ARP userspace stack demonstration. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define MULIB_IMPL_UIP_ARP
#define MULIB_IMPL_UIP_ARP_CACHE
#define MULIB_IMPL_UIP_CHECKSUM
#define MULIB_IMPL_UIP_ETHERNET
#define MULIB_IMPL_UIP_ICMP
#define MULIB_IMPL_UIP_IPV4
#define MULIB_IMPL_UIP_UDP
#define MULIB_IMPL_USTACK
#include "mulib-ustack.h"
static int
send_frame (const void *frame, size_t n, void *p)
{
  (void)frame;
  (*(int *)p)++;
  printf ("Emitted %zu-byte Ethernet frame\n", n);
  return 0;
}
int
main (void)
{
  const uint8_t mac[6] = { 2, 0, 0, 0, 0, 1 }, peer[6] = { 2, 0, 0, 0, 0, 2 };
  const uint8_t ip[4] = { 10, 1, 2, 3 }, netmask[4] = { 255, 255, 255, 0 },
                peer_ip[4] = { 10, 1, 2, 4 };
  const uint8_t broadcast[6] = { 255, 255, 255, 255, 255, 255 };
  mulib_ustack_t stack;
  int emitted = 0;
  if (mulib_ustack_init (&stack, mac, ip, netmask, NULL, send_frame, NULL,
                         &emitted))
    return 1;
  mulib_uip_arp_packet_t arp = { 0 };
  arp.operation = 1;
  memcpy (arp.sender_mac, peer, 6);
  memcpy (arp.sender_ip, peer_ip, 4);
  memcpy (arp.target_ip, ip, 4);
  uint8_t arppkt[28], ethernet[64];
  size_t used = 0;
  if (mulib_uip_arp_pack (arppkt, sizeof arppkt, &arp) < 0)
    return 1;
  if (mulib_uip_ethernet_pack (ethernet, sizeof ethernet, broadcast, peer,
                               0x0806, arppkt, sizeof arppkt, &used))
    return 1;
  if (mulib_ustack_process (&stack, ethernet, used))
    return 1;
  printf ("Offline test emitted %d ARP reply\n", emitted);
  return emitted != 1;
}
