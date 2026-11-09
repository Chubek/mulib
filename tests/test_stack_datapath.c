
#include <assert.h>
#include <errno.h>
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

struct captures
{
  uint8_t last[2048];
  size_t len;
  unsigned sent, received;
};
static int
emit (const void *p, size_t n, void *arg)
{
  struct captures *c = arg;
  assert (n <= sizeof c->last);
  memcpy (c->last, p, n);
  c->len = n;
  c->sent++;
  return 0;
}
static int
udp (const uint8_t src[4], uint16_t sp, uint16_t dp, const uint8_t *p,
     size_t n, void *arg)
{
  struct captures *c = arg;
  assert (src[0] == 10 && sp == 3456 && dp == 7777 && n == 3
          && !memcmp (p, "hey", 3));
  c->received++;
  return 0;
}
int
main (void)
{
  const uint8_t own_mac[6] = { 2, 0, 0, 0, 0, 2 },
                peer_mac[6] = { 2, 0, 0, 0, 0, 1 },
                broadcast[6] = { 255, 255, 255, 255, 255, 255 };
  const uint8_t own_ip[4] = { 10, 0, 0, 2 }, peer_ip[4] = { 10, 0, 0, 1 },
                mask[4] = { 255, 255, 255, 0 };
  struct captures cap = { 0 };
  mulib_ustack_t s;
  assert (
      !mulib_ustack_init (&s, own_mac, own_ip, mask, NULL, emit, udp, &cap));
  mulib_ustack_tick (&s, 1000);
  mulib_uip_arp_packet_t arp = { 0 };
  arp.operation = 1;
  memcpy (arp.sender_mac, peer_mac, 6);
  memcpy (arp.sender_ip, peer_ip, 4);
  memcpy (arp.target_ip, own_ip, 4);
  uint8_t arpb[28], eth[1514];
  size_t elen = 0;
  assert (mulib_uip_arp_pack (arpb, sizeof arpb, &arp) == 28);
  assert (!mulib_uip_ethernet_pack (eth, sizeof eth, broadcast, peer_mac,
                                    0x0806, arpb, 28, &elen));
  assert (!mulib_ustack_process (&s, eth, elen));
  assert (cap.sent == 1);
  mulib_uip_ethernet_frame_t frame;
  assert (!mulib_uip_ethernet_parse (cap.last, cap.len, &frame));
  assert (frame.ethertype == 0x0806);
  mulib_uip_arp_packet_t answer;
  assert (!mulib_uip_arp_parse (frame.payload, frame.length, &answer));
  assert (answer.operation == 2);
  assert (!memcmp (answer.target_ip, peer_ip, 4));
  // Deliver inbound UDP into the registered application callback.
  uint8_t udpbuf[32], ipbuf[1500];
  size_t un = 0, pn = 0;
  assert (!mulib_uip_udp_pack (udpbuf, sizeof udpbuf, peer_ip, own_ip, 3456,
                               7777, "hey", 3, &un));
  assert (!mulib_uip_ipv4_pack (ipbuf, sizeof ipbuf, peer_ip, own_ip, 17, 64,
                                3, udpbuf, un, &pn));
  assert (!mulib_uip_ethernet_pack (eth, sizeof eth, own_mac, peer_mac, 0x0800,
                                    ipbuf, pn, &elen));
  assert (!mulib_ustack_process (&s, eth, elen));
  assert (cap.received == 1);
  // ICMPv4 request elicits checksum-valid echo reply.
  uint8_t icmp[32];
  size_t in = 0;
  assert (!mulib_uip_icmp_echo (icmp, sizeof icmp, 0, 99, 1, "abc", 3, &in));
  assert (!mulib_uip_ipv4_pack (ipbuf, sizeof ipbuf, peer_ip, own_ip, 1, 64, 4,
                                icmp, in, &pn));
  assert (!mulib_uip_ethernet_pack (eth, sizeof eth, own_mac, peer_mac, 0x0800,
                                    ipbuf, pn, &elen));
  assert (!mulib_ustack_process (&s, eth, elen));
  assert (cap.sent == 2);
  assert (!mulib_uip_ethernet_parse (cap.last, cap.len, &frame));
  mulib_uip_ipv4_packet_t ip;
  assert (!mulib_uip_ipv4_parse (frame.payload, frame.length, &ip));
  assert (ip.protocol == 1 && !memcmp (ip.dst, peer_ip, 4)
          && ip.payload[0] == 0);
  assert (!mulib_uip_icmp_verify (ip.payload, ip.payload_length));
  // Peer ARP response populates cache for outgoing UDP.
  arp.operation = 2;
  memcpy (arp.target_ip, own_ip, 4);
  memcpy (arp.target_mac, own_mac, 6);
  assert (mulib_uip_arp_pack (arpb, sizeof arpb, &arp) == 28);
  assert (!mulib_uip_ethernet_pack (eth, sizeof eth, own_mac, peer_mac, 0x0806,
                                    arpb, 28, &elen));
  assert (!mulib_ustack_process (&s, eth, elen));
  assert (!mulib_ustack_send_udp (&s, peer_ip, 1111, 2222, "hi", 2));
  assert (cap.sent == 3);
  assert (!mulib_uip_ethernet_parse (cap.last, cap.len, &frame));
  assert (!mulib_uip_ipv4_parse (frame.payload, frame.length, &ip));
  assert (ip.protocol == 17);
  mulib_uip_udp_datagram_t result;
  assert (!mulib_uip_udp_parse (ip.src, ip.dst, ip.payload, ip.payload_length,
                                &result));
  assert (result.dst_port == 2222);
  // Unresolved neighbor triggers ARP request rather than fabricated frame
  // delivery.
  const uint8_t unknown_ip[4] = { 10, 0, 0, 5 };
  assert (mulib_ustack_send_udp (&s, unknown_ip, 1111, 2222, "hi", 2)
          == -EAGAIN);
  assert (cap.sent == 4);
  assert (!mulib_uip_ethernet_parse (cap.last, cap.len, &frame));
  assert (frame.ethertype == 0x0806);
  puts ("PASS stack datapath: ARP, ICMP reply, inbound/outbound UDP, "
        "unresolved ARP");
  return 0;
}
