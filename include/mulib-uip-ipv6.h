/** @file mulib-uip-ipv6.h
 * @brief Build and parse basic IPv6 fixed headers without extension-header
 * processing C11, single-header, define MULIB_IMPL_UIP_IPV6 exactly once.
 */
#ifndef MULIB_UIP_IPV6_H
#define MULIB_UIP_IPV6_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_uip_ipv6_packet
  {
    uint8_t src[16], dst[16], next_header, hop_limit;
    const uint8_t *payload;
    size_t payload_length;
  } mulib_uip_ipv6_packet_t;
  int mulib_uip_ipv6_parse (const void *packet, size_t length,
                            mulib_uip_ipv6_packet_t *out);
  int mulib_uip_ipv6_pack (void *out, size_t cap, const uint8_t src[16],
                           const uint8_t dst[16], uint8_t next_header,
                           uint8_t hops, const void *data, size_t n,
                           size_t *used);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UIP_IPV6) && !defined(MULIB_UIP_IPV6_IMPLEMENTED)
#define MULIB_UIP_IPV6_IMPLEMENTED
#include <errno.h>
#include <string.h>
int
mulib_uip_ipv6_parse (const void *p, size_t n, mulib_uip_ipv6_packet_t *out)
{
  if (!p || !out || n < 40)
    return -EINVAL;
  const uint8_t *b = p;
  size_t len = (size_t)b[4] * 256u + b[5];
  if ((b[0] >> 4) != 6 || len > n - 40)
    return -EBADMSG;
  mulib_uip_ipv6_packet_t v = { 0 };
  v.next_header = b[6];
  v.hop_limit = b[7];
  memcpy (v.src, b + 8, 16);
  memcpy (v.dst, b + 24, 16);
  v.payload = b + 40;
  v.payload_length = len;
  *out = v;
  return 0;
}
int
mulib_uip_ipv6_pack (void *p, size_t cap, const uint8_t src[16],
                     const uint8_t dst[16], uint8_t next, uint8_t hops,
                     const void *payload, size_t n, size_t *used)
{
  if (!p || !src || !dst || (!payload && n) || !used || n > 65535
      || cap < 40 + n)
    return -EINVAL;
  uint8_t *b = p;
  memset (b, 0, 40);
  b[0] = 0x60;
  b[4] = (uint8_t)(n >> 8);
  b[5] = (uint8_t)n;
  b[6] = next;
  b[7] = hops;
  memcpy (b + 8, src, 16);
  memcpy (b + 24, dst, 16);
  if (n)
    memcpy (b + 40, payload, n);
  *used = 40 + n;
  return 0;
}
#endif
