/** @file mulib-uip-ipv4.h
 * @brief Build and parse verified unfragmented IPv4 packets and expose
 * fragmentation flags C11, single-header, define MULIB_IMPL_UIP_IPV4 exactly
 * once.
 */
#ifndef MULIB_UIP_IPV4_H
#define MULIB_UIP_IPV4_H
#include "mulib-uip-checksum.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_uip_ipv4_packet
  {
    uint8_t src[4], dst[4], protocol, ttl;
    uint16_t id, fragment;
    const uint8_t *payload;
    size_t payload_length, header_length;
  } mulib_uip_ipv4_packet_t;
  int mulib_uip_ipv4_parse (const void *packet, size_t length,
                            mulib_uip_ipv4_packet_t *out);
  int mulib_uip_ipv4_pack (void *out, size_t cap, const uint8_t src[4],
                           const uint8_t dst[4], uint8_t protocol, uint8_t ttl,
                           uint16_t id, const void *payload, size_t n,
                           size_t *used);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UIP_IPV4) && !defined(MULIB_UIP_IPV4_IMPLEMENTED)
#define MULIB_UIP_IPV4_IMPLEMENTED
#include <errno.h>
#include <string.h>
int
mulib_uip_ipv4_parse (const void *p, size_t n, mulib_uip_ipv4_packet_t *out)
{
  if (!p || !out || n < 20)
    return -EINVAL;
  const uint8_t *b = p;
  size_t h = (b[0] & 15u) * 4u;
  size_t total = (size_t)b[2] * 256u + b[3];
  if ((b[0] >> 4) != 4 || h < 20 || h > n || h > total || total > n
      || mulib_uip_checksum (b, h))
    return -EBADMSG;
  mulib_uip_ipv4_packet_t v = { 0 };
  memcpy (v.src, b + 12, 4);
  memcpy (v.dst, b + 16, 4);
  v.protocol = b[9];
  v.ttl = b[8];
  v.id = (uint16_t)(b[4] << 8 | b[5]);
  v.fragment = (uint16_t)(b[6] << 8 | b[7]);
  v.payload = b + h;
  v.payload_length = total - h;
  v.header_length = h;
  *out = v;
  return 0;
}
int
mulib_uip_ipv4_pack (void *out, size_t cap, const uint8_t src[4],
                     const uint8_t dst[4], uint8_t proto, uint8_t ttl,
                     uint16_t id, const void *payload, size_t n, size_t *used)
{
  if (!out || !src || !dst || (!payload && n) || !used || n > 65515
      || cap < 20 + n)
    return -EINVAL;
  uint8_t *b = out;
  memset (b, 0, 20);
  b[0] = 0x45;
  b[2] = (uint8_t)((n + 20) >> 8);
  b[3] = (uint8_t)(n + 20);
  b[4] = (uint8_t)(id >> 8);
  b[5] = (uint8_t)id;
  b[6] = 0x40;
  b[8] = ttl;
  b[9] = proto;
  memcpy (b + 12, src, 4);
  memcpy (b + 16, dst, 4);
  uint16_t sum = mulib_uip_checksum (b, 20);
  b[10] = (uint8_t)(sum >> 8);
  b[11] = (uint8_t)sum;
  if (n)
    memcpy (b + 20, payload, n);
  *used = n + 20;
  return 0;
}
#endif
