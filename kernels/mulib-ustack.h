/** @file mulib-ustack.h
 * @brief Small userspace Ethernet/ARP/IPv4/ICMP echo/UDP endpoint with
 * callback-driven I/O C11, single-header, define MULIB_IMPL_USTACK exactly
 * once.
 */
#ifndef MULIB_USTACK_H
#define MULIB_USTACK_H
#include "mulib-uip-arp-cache.h"
#include "mulib-uip-arp.h"
#include "mulib-uip-ethernet.h"
#include "mulib-uip-icmp.h"
#include "mulib-uip-ipv4.h"
#include "mulib-uip-udp.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef int (*mulib_ustack_emit_fn) (const void *ethernet, size_t length,
                                       void *context);
  typedef int (*mulib_ustack_udp_fn) (const uint8_t source[4],
                                      uint16_t src_port, uint16_t dst_port,
                                      const uint8_t *payload, size_t length,
                                      void *context);
  typedef struct mulib_ustack
  {
    uint8_t mac[6], ip[4], netmask[4], gateway[4];
    mulib_uip_arp_cache_t arp;
    uint16_t next_id;
    uint64_t now;
    mulib_ustack_emit_fn emit;
    mulib_ustack_udp_fn udp;
    void *context;
  } mulib_ustack_t;
  int mulib_ustack_init (mulib_ustack_t *s, const uint8_t mac[6],
                         const uint8_t ip[4], const uint8_t mask[4],
                         const uint8_t gateway[4], mulib_ustack_emit_fn emit,
                         mulib_ustack_udp_fn udp, void *ctx);
  int mulib_ustack_process (mulib_ustack_t *s, const void *frame,
                            size_t length);
  int mulib_ustack_send_udp (mulib_ustack_t *s, const uint8_t dst[4],
                             uint16_t src_port, uint16_t dst_port,
                             const void *payload, size_t length);
  void mulib_ustack_tick (mulib_ustack_t *s, uint64_t now);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_USTACK) && !defined(MULIB_USTACK_IMPLEMENTED)
#define MULIB_USTACK_IMPLEMENTED
#include <errno.h>
#include <string.h>
static const uint8_t mulib_ustack_bcast_[6] = { 255, 255, 255, 255, 255, 255 };
int
mulib_ustack_init (mulib_ustack_t *s, const uint8_t mac[6],
                   const uint8_t ip[4], const uint8_t mask[4],
                   const uint8_t gateway[4], mulib_ustack_emit_fn emit,
                   mulib_ustack_udp_fn udp, void *ctx)
{
  if (!s || !mac || !ip || !mask || !emit)
    return -EINVAL;
  memset (s, 0, sizeof *s);
  memcpy (s->mac, mac, 6);
  memcpy (s->ip, ip, 4);
  memcpy (s->netmask, mask, 4);
  if (gateway)
    memcpy (s->gateway, gateway, 4);
  s->emit = emit;
  s->udp = udp;
  s->context = ctx;
  s->next_id = 1;
  return 0;
}
void
mulib_ustack_tick (mulib_ustack_t *s, uint64_t now)
{
  if (s)
    {
      s->now = now;
      mulib_uip_arp_cache_expire (&s->arp, now);
    }
}
static int
mulib_ustack_arp_request_ (mulib_ustack_t *s, const uint8_t target[4])
{
  mulib_uip_arp_packet_t arp = { 0 };
  arp.operation = 1;
  memcpy (arp.sender_mac, s->mac, 6);
  memcpy (arp.sender_ip, s->ip, 4);
  memcpy (arp.target_ip, target, 4);
  uint8_t data[28], frame[42];
  size_t n = 0;
  int r = mulib_uip_arp_pack (data, sizeof data, &arp);
  if (r < 0)
    return r;
  r = mulib_uip_ethernet_pack (frame, sizeof frame, mulib_ustack_bcast_,
                               s->mac, 0x0806, data, sizeof data, &n);
  return r ? r : s->emit (frame, n, s->context);
}
int
mulib_ustack_process (mulib_ustack_t *s, const void *buf, size_t size)
{
  if (!s || !buf || !s->emit)
    return -EINVAL;
  mulib_uip_ethernet_frame_t e;
  int r = mulib_uip_ethernet_parse (buf, size, &e);
  if (r)
    return r;
  if (memcmp (e.dst, s->mac, 6) && memcmp (e.dst, mulib_ustack_bcast_, 6))
    return 0;
  if (e.ethertype == 0x0806)
    {
      mulib_uip_arp_packet_t a;
      r = mulib_uip_arp_parse (e.payload, e.length, &a);
      if (r)
        return r;
      if (a.operation == 2)
        {
          (void)mulib_uip_arp_cache_store (&s->arp, a.sender_ip, a.sender_mac,
                                           s->now, 300000);
          return 0;
        }
      if (a.operation != 1 || memcmp (a.target_ip, s->ip, 4))
        return 0;
      uint8_t payload[28], frame[42], requester[6];
      size_t used = 0;
      memcpy (requester, a.sender_mac, 6);
      memcpy (a.target_mac, requester, 6);
      memcpy (a.target_ip, a.sender_ip, 4);
      memcpy (a.sender_mac, s->mac, 6);
      memcpy (a.sender_ip, s->ip, 4);
      a.operation = 2;
      r = mulib_uip_arp_pack (payload, sizeof payload, &a);
      if (r < 0)
        return r;
      r = mulib_uip_ethernet_pack (frame, sizeof frame, requester, s->mac,
                                   0x0806, payload, sizeof payload, &used);
      return r ? r : s->emit (frame, used, s->context);
    }
  if (e.ethertype != 0x0800)
    return 0;
  mulib_uip_ipv4_packet_t ip;
  r = mulib_uip_ipv4_parse (e.payload, e.length, &ip);
  if (r)
    return r;
  if (memcmp (ip.dst, s->ip, 4))
    return 0;
  if (ip.fragment & 0x3fff)
    return -EOPNOTSUPP;
  if (ip.protocol == 1)
    {
      if (ip.payload_length < 8
          || mulib_uip_icmp_verify (ip.payload, ip.payload_length))
        return -EBADMSG;
      if (ip.payload[0] != 8 || ip.payload[1])
        return 0;
      uint8_t icmp[1480], datagram[1500], frame[1514];
      size_t icn = 0, ipn = 0, frn = 0;
      if (ip.payload_length > sizeof icmp)
        return -EMSGSIZE;
      uint16_t id = (uint16_t)(ip.payload[4] << 8 | ip.payload[5]);
      uint16_t seq = (uint16_t)(ip.payload[6] << 8 | ip.payload[7]);
      r = mulib_uip_icmp_echo (icmp, sizeof icmp, 1, id, seq, ip.payload + 8,
                               ip.payload_length - 8, &icn);
      if (r)
        return r;
      r = mulib_uip_ipv4_pack (datagram, sizeof datagram, s->ip, ip.src, 1, 64,
                               s->next_id++, icmp, icn, &ipn);
      if (r)
        return r;
      r = mulib_uip_ethernet_pack (frame, sizeof frame, e.src, s->mac, 0x0800,
                                   datagram, ipn, &frn);
      return r ? r : s->emit (frame, frn, s->context);
    }
  if (ip.protocol == 17)
    {
      mulib_uip_udp_datagram_t udp;
      r = mulib_uip_udp_parse (ip.src, ip.dst, ip.payload, ip.payload_length,
                               &udp);
      if (r)
        return r;
      return s->udp ? s->udp (ip.src, udp.src_port, udp.dst_port, udp.payload,
                              udp.length, s->context)
                    : 0;
    }
  return 0;
}
int
mulib_ustack_send_udp (mulib_ustack_t *s, const uint8_t dst[4],
                       uint16_t source_port, uint16_t dest_port,
                       const void *data, size_t n)
{
  if (!s || !s->emit || !dst || (!data && n) || n > 1472)
    return -EINVAL;
  uint8_t hop[4], mac[6];
  int onlink = 1;
  for (int i = 0; i < 4; i++)
    if ((dst[i] & s->netmask[i]) != (s->ip[i] & s->netmask[i]))
      onlink = 0;
  memcpy (hop, onlink ? dst : s->gateway, 4);
  if (!onlink && hop[0] == 0 && hop[1] == 0 && hop[2] == 0 && hop[3] == 0)
    return -ENETUNREACH;
  if (mulib_uip_arp_cache_find (&s->arp, hop, s->now, mac))
    {
      int r = mulib_ustack_arp_request_ (s, hop);
      return r ? r : -EAGAIN;
    }
  uint8_t udp[1480], packet[1500], frame[1514];
  size_t udpn = 0, ipn = 0, frn = 0;
  int r = mulib_uip_udp_pack (udp, sizeof udp, s->ip, dst, source_port,
                              dest_port, data, n, &udpn);
  if (r)
    return r;
  r = mulib_uip_ipv4_pack (packet, sizeof packet, s->ip, dst, 17, 64,
                           s->next_id++, udp, udpn, &ipn);
  if (r)
    return r;
  r = mulib_uip_ethernet_pack (frame, sizeof frame, mac, s->mac, 0x0800,
                               packet, ipn, &frn);
  return r ? r : s->emit (frame, frn, s->context);
}
#endif
