/** @file mulib-uip-icmp.h
 * @brief Build verified ICMPv4 echo requests or replies for userspace IPv4
 * C11, single-header, define MULIB_IMPL_UIP_ICMP exactly once.
 */
#ifndef MULIB_UIP_ICMP_H
#define MULIB_UIP_ICMP_H
#include "mulib-uip-checksum.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_uip_icmp_echo (void *out, size_t cap, int reply, uint16_t id,
                           uint16_t sequence, const void *data, size_t n,
                           size_t *used);
  int mulib_uip_icmp_verify (const void *pkt, size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UIP_ICMP) && !defined(MULIB_UIP_ICMP_IMPLEMENTED)
#define MULIB_UIP_ICMP_IMPLEMENTED
#include <errno.h>
#include <string.h>
int
mulib_uip_icmp_echo (void *out, size_t cap, int reply, uint16_t id,
                     uint16_t seq, const void *data, size_t n, size_t *used)
{
  if (!out || !used || (!data && n) || n > 65527 || cap < n + 8
      || (reply != 0 && reply != 1))
    return -EINVAL;
  uint8_t *b = out;
  b[0] = reply ? 0 : 8;
  b[1] = 0;
  b[2] = b[3] = 0;
  b[4] = (uint8_t)(id >> 8);
  b[5] = (uint8_t)id;
  b[6] = (uint8_t)(seq >> 8);
  b[7] = (uint8_t)seq;
  if (n)
    memcpy (b + 8, data, n);
  uint16_t sum = mulib_uip_checksum (b, n + 8);
  b[2] = (uint8_t)(sum >> 8);
  b[3] = (uint8_t)sum;
  *used = n + 8;
  return 0;
}
int
mulib_uip_icmp_verify (const void *p, size_t n)
{
  if (!p || n < 8)
    return -EINVAL;
  return mulib_uip_checksum (p, n) ? -EBADMSG : 0;
}
#endif
