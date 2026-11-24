/** @file mulib-uip-checksum.h
 * @brief RFC 1071 Internet checksum and IPv4 TCP/UDP pseudoheader checksum
 * C11, single-header, define MULIB_IMPL_UIP_CHECKSUM exactly once.
 */
#ifndef MULIB_UIP_CHECKSUM_H
#define MULIB_UIP_CHECKSUM_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  uint16_t mulib_uip_checksum (const void *data, size_t size);
  uint16_t mulib_uip_checksum_ipv4 (const uint8_t src[4], const uint8_t dst[4],
                                    uint8_t proto, const void *segment,
                                    size_t size);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UIP_CHECKSUM)                                          \
    && !defined(MULIB_UIP_CHECKSUM_IMPLEMENTED)
#define MULIB_UIP_CHECKSUM_IMPLEMENTED
#include <stddef.h>
static uint32_t
mulib_uip_sum_ (const uint8_t *b, size_t n, uint32_t sum)
{
  size_t i = 0;
  for (; i < n && n - i > 1; i += 2)
    {
      sum += (uint32_t)b[i] * 256u + b[i + 1];
      sum = (sum & 65535u) + (sum >> 16);
    }
  if (i < n)
    sum += (uint32_t)b[i] * 256u;
  return sum;
}
static uint16_t
mulib_uip_finish_ (uint32_t sum)
{
  while (sum >> 16)
    sum = (sum & 65535u) + (sum >> 16);
  return (uint16_t)(~sum);
}
uint16_t
mulib_uip_checksum (const void *p, size_t n)
{
  if (!p && n)
    return 0;
  return mulib_uip_finish_ (mulib_uip_sum_ (p, n, 0));
}
uint16_t
mulib_uip_checksum_ipv4 (const uint8_t s[4], const uint8_t d[4], uint8_t proto,
                         const void *p, size_t n)
{
  if (!s || !d || (!p && n) || n > 65535)
    return 0;
  uint32_t sum = mulib_uip_sum_ (s, 4, 0);
  sum = mulib_uip_sum_ (d, 4, sum);
  sum += proto + (uint32_t)n;
  return mulib_uip_finish_ (mulib_uip_sum_ (p, n, sum));
}
#endif
