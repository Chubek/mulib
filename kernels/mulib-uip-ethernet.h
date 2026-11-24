/** @file mulib-uip-ethernet.h
 * @brief Serialize and validate fixed-length Ethernet II frames
 * C11, single-header, define MULIB_IMPL_UIP_ETHERNET exactly once.
 */
#ifndef MULIB_UIP_ETHERNET_H
#define MULIB_UIP_ETHERNET_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_uip_ethernet_frame
  {
    uint8_t dst[6], src[6];
    uint16_t ethertype;
    const uint8_t *payload;
    size_t length;
  } mulib_uip_ethernet_frame_t;
  int mulib_uip_ethernet_parse (const void *frame, size_t length,
                                mulib_uip_ethernet_frame_t *out);
  int mulib_uip_ethernet_pack (void *frame, size_t cap, const uint8_t dst[6],
                               const uint8_t src[6], uint16_t ethertype,
                               const void *payload, size_t n, size_t *used);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UIP_ETHERNET)                                          \
    && !defined(MULIB_UIP_ETHERNET_IMPLEMENTED)
#define MULIB_UIP_ETHERNET_IMPLEMENTED
#include <errno.h>
#include <string.h>
int
mulib_uip_ethernet_parse (const void *p, size_t n,
                          mulib_uip_ethernet_frame_t *out)
{
  if (!p || !out || n < 14)
    return -EINVAL;
  const uint8_t *b = p;
  mulib_uip_ethernet_frame_t f = { 0 };
  memcpy (f.dst, b, 6);
  memcpy (f.src, b + 6, 6);
  f.ethertype = (uint16_t)(b[12] << 8 | b[13]);
  f.payload = b + 14;
  f.length = n - 14;
  *out = f;
  return 0;
}
int
mulib_uip_ethernet_pack (void *out, size_t cap, const uint8_t dst[6],
                         const uint8_t src[6], uint16_t et, const void *p,
                         size_t n, size_t *used)
{
  if (!out || !dst || !src || (!p && n) || !used || n > cap || cap - n < 14)
    return -EINVAL;
  uint8_t *b = out;
  memcpy (b, dst, 6);
  memcpy (b + 6, src, 6);
  b[12] = (uint8_t)(et >> 8);
  b[13] = (uint8_t)et;
  if (n)
    memcpy (b + 14, p, n);
  *used = 14 + n;
  return 0;
}
#endif
