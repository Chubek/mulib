/** @file mulib-icmp.h
 * @brief Compose and verify ICMP echo packets, with ones-complement checksum
 * C11, single-header, define MULIB_IMPL_ICMP exactly once.
 */
#ifndef MULIB_ICMP_H
#define MULIB_ICMP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  uint16_t mulib_icmp_checksum (const void *data, size_t length);
  int mulib_icmp_echo (void *buffer, size_t cap, uint8_t type, uint16_t id,
                       uint16_t sequence, const void *payload,
                       size_t payload_size, size_t *written);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ICMP) && !defined(MULIB_ICMP_IMPLEMENTED)
#define MULIB_ICMP_IMPLEMENTED
#include <errno.h>
#include <string.h>
uint16_t
mulib_icmp_checksum (const void *p, size_t n)
{
  const uint8_t *b = p;
  uint32_t sum = 0;
  if (!p && n)
    return 0;
  for (size_t i = 0; i < n && n - i > 1; i += 2)
    {
      sum += ((uint16_t)b[i] << 8) | b[i + 1];
      sum = (sum & 65535u) + (sum >> 16);
    }
  if (n & 1)
    sum += (uint16_t)b[n - 1] << 8;
  while (sum >> 16)
    sum = (sum & 65535u) + (sum >> 16);
  return (uint16_t)~sum;
}
int
mulib_icmp_echo (void *buf, size_t cap, uint8_t type, uint16_t id,
                 uint16_t seq, const void *payload, size_t pn, size_t *out)
{
  if (!buf || !out || (!payload && pn) || pn > 65527 || cap < 8 + pn)
    return -EINVAL;
  uint8_t *b = buf;
  b[0] = type;
  b[1] = 0;
  b[2] = b[3] = 0;
  b[4] = (uint8_t)(id >> 8);
  b[5] = (uint8_t)id;
  b[6] = (uint8_t)(seq >> 8);
  b[7] = (uint8_t)seq;
  if (pn)
    memcpy (b + 8, payload, pn);
  uint16_t check = mulib_icmp_checksum (b, 8 + pn);
  b[2] = (uint8_t)(check >> 8);
  b[3] = (uint8_t)check;
  *out = 8 + pn;
  return 0;
}
#endif
