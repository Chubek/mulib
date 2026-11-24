/** @file mulib-uip-arp.h
 * @brief Encode/decode Ethernet IPv4 ARP request and reply packets
 * C11, single-header, define MULIB_IMPL_UIP_ARP exactly once.
 */
#ifndef MULIB_UIP_ARP_H
#define MULIB_UIP_ARP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_uip_arp_packet
  {
    uint16_t operation;
    uint8_t sender_mac[6], sender_ip[4], target_mac[6], target_ip[4];
  } mulib_uip_arp_packet_t;
  int mulib_uip_arp_parse (const void *bytes, size_t n,
                           mulib_uip_arp_packet_t *out);
  int mulib_uip_arp_pack (void *bytes, size_t cap,
                          const mulib_uip_arp_packet_t *arp);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UIP_ARP) && !defined(MULIB_UIP_ARP_IMPLEMENTED)
#define MULIB_UIP_ARP_IMPLEMENTED
#include <errno.h>
#include <string.h>
int
mulib_uip_arp_parse (const void *p, size_t n, mulib_uip_arp_packet_t *out)
{
  if (!p || !out || n < 28)
    return -EINVAL;
  const uint8_t *b = p;
  if (b[0] != 0 || b[1] != 1 || b[2] != 8 || b[3] != 0 || b[4] != 6
      || b[5] != 4 || b[6] != 0 || (b[7] != 1 && b[7] != 2))
    return -EBADMSG;
  mulib_uip_arp_packet_t v = { 0 };
  v.operation = b[7];
  memcpy (v.sender_mac, b + 8, 6);
  memcpy (v.sender_ip, b + 14, 4);
  memcpy (v.target_mac, b + 18, 6);
  memcpy (v.target_ip, b + 24, 4);
  *out = v;
  return 0;
}
int
mulib_uip_arp_pack (void *p, size_t n, const mulib_uip_arp_packet_t *a)
{
  if (!p || !a || n < 28 || (a->operation != 1 && a->operation != 2))
    return -EINVAL;
  uint8_t *b = p;
  memset (b, 0, 28);
  b[1] = 1;
  b[2] = 8;
  b[4] = 6;
  b[5] = 4;
  b[7] = (uint8_t)a->operation;
  memcpy (b + 8, a->sender_mac, 6);
  memcpy (b + 14, a->sender_ip, 4);
  memcpy (b + 18, a->target_mac, 6);
  memcpy (b + 24, a->target_ip, 4);
  return 28;
}
#endif
