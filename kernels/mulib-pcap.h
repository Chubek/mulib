/** @file mulib-pcap.h
 * @brief Write classic little-endian PCAP stream headers and timestamped
 * capture records C11, single-header, define MULIB_IMPL_PCAP exactly once.
 */
#ifndef MULIB_PCAP_H
#define MULIB_PCAP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_pcap_write_global (void *buf, size_t n, uint32_t snaplen,
                               uint32_t linktype);
  int mulib_pcap_write_record (void *buf, size_t cap, uint32_t seconds,
                               uint32_t micros, const void *pkt, size_t length,
                               size_t *written);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCAP) && !defined(MULIB_PCAP_IMPLEMENTED)
#define MULIB_PCAP_IMPLEMENTED
#include <errno.h>
#include <string.h>
static void
mulib_pcap_u32_ (uint8_t *b, uint32_t v)
{
  for (unsigned i = 0; i < 4; i++)
    b[i] = (uint8_t)(v >> (8 * i));
}
int
mulib_pcap_write_global (void *buf, size_t n, uint32_t snap, uint32_t link)
{
  if (!buf || n < 24 || !snap)
    return -EINVAL;
  uint8_t *b = buf;
  memset (b, 0, 24);
  mulib_pcap_u32_ (b, 0xa1b2c3d4);
  b[4] = 2;
  b[6] = 4;
  mulib_pcap_u32_ (b + 16, snap);
  mulib_pcap_u32_ (b + 20, link);
  return 24;
}
int
mulib_pcap_write_record (void *buf, size_t cap, uint32_t sec, uint32_t usec,
                         const void *pkt, size_t n, size_t *used)
{
  if (!buf || !used || (!pkt && n) || n > UINT32_MAX || cap < 16
      || n > cap - 16 || usec >= 1000000)
    return -EINVAL;
  uint8_t *b = buf;
  mulib_pcap_u32_ (b, sec);
  mulib_pcap_u32_ (b + 4, usec);
  mulib_pcap_u32_ (b + 8, (uint32_t)n);
  mulib_pcap_u32_ (b + 12, (uint32_t)n);
  if (n)
    memcpy (b + 16, pkt, n);
  *used = 16 + n;
  return 0;
}
#endif
