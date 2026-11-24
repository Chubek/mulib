/** @file mulib-uip-udp.h
 * @brief Build/check IPv4 UDP datagrams with correct pseudoheader checksums
 * C11, single-header, define MULIB_IMPL_UIP_UDP exactly once.
 */
#ifndef MULIB_UIP_UDP_H
#define MULIB_UIP_UDP_H
#include "mulib-uip-checksum.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_uip_udp_datagram
  {
    uint16_t src_port, dst_port;
    const uint8_t *payload;
    size_t length;
  } mulib_uip_udp_datagram_t;
  int mulib_uip_udp_parse (const uint8_t src_ip[4], const uint8_t dst_ip[4],
                           const void *pkt, size_t n,
                           mulib_uip_udp_datagram_t *out);
  int mulib_uip_udp_pack (void *pkt, size_t cap, const uint8_t src_ip[4],
                          const uint8_t dst_ip[4], uint16_t src_port,
                          uint16_t dst_port, const void *payload, size_t n,
                          size_t *used);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UIP_UDP) && !defined(MULIB_UIP_UDP_IMPLEMENTED)
#define MULIB_UIP_UDP_IMPLEMENTED
#include <errno.h>
#include <string.h>
int
mulib_uip_udp_parse (const uint8_t src[4], const uint8_t dst[4], const void *p,
                     size_t n, mulib_uip_udp_datagram_t *out)
{
  if (!src || !dst || !p || !out || n < 8 || n > 65535)
    return -EINVAL;
  const uint8_t *b = p;
  size_t len = (size_t)b[4] * 256u + b[5];
  if (len < 8 || len > n)
    return -EBADMSG;
  if ((b[6] || b[7]) && mulib_uip_checksum_ipv4 (src, dst, 17, b, len))
    return -EBADMSG;
  mulib_uip_udp_datagram_t u
      = { (uint16_t)(b[0] << 8 | b[1]), (uint16_t)(b[2] << 8 | b[3]), b + 8,
          len - 8 };
  *out = u;
  return 0;
}
int
mulib_uip_udp_pack (void *p, size_t cap, const uint8_t src[4],
                    const uint8_t dst[4], uint16_t sp, uint16_t dp,
                    const void *payload, size_t n, size_t *used)
{
  if (!p || !src || !dst || (!payload && n) || !used || n > 65527
      || cap < 8 + n)
    return -EINVAL;
  uint8_t *b = p;
  b[0] = (uint8_t)(sp >> 8);
  b[1] = (uint8_t)sp;
  b[2] = (uint8_t)(dp >> 8);
  b[3] = (uint8_t)dp;
  b[4] = (uint8_t)((n + 8) >> 8);
  b[5] = (uint8_t)(n + 8);
  b[6] = b[7] = 0;
  if (n)
    memcpy (b + 8, payload, n);
  uint16_t sum = mulib_uip_checksum_ipv4 (src, dst, 17, b, 8 + n);
  if (!sum)
    sum = 0xffff;
  b[6] = (uint8_t)(sum >> 8);
  b[7] = (uint8_t)sum;
  *used = 8 + n;
  return 0;
}
#endif
