/** @file mulib-uip-tcp.h
 * @brief Parse basic TCP headers and encode TCP no-option IPv4 segments with
 * checksum C11, single-header, define MULIB_IMPL_UIP_TCP exactly once.
 */
#ifndef MULIB_UIP_TCP_H
#define MULIB_UIP_TCP_H
#include "mulib-uip-checksum.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_uip_tcp_segment
  {
    uint16_t source_port, dest_port, window;
    uint32_t sequence, acknowledgment;
    uint8_t flags;
    const uint8_t *data;
    size_t length;
  } mulib_uip_tcp_segment_t;
  int mulib_uip_tcp_parse (const uint8_t src[4], const uint8_t dst[4],
                           const void *packet, size_t n,
                           mulib_uip_tcp_segment_t *out);
  int mulib_uip_tcp_pack (void *out, size_t capacity, const uint8_t src[4],
                          const uint8_t dst[4],
                          const mulib_uip_tcp_segment_t *segment,
                          size_t *used);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UIP_TCP) && !defined(MULIB_UIP_TCP_IMPLEMENTED)
#define MULIB_UIP_TCP_IMPLEMENTED
#include <errno.h>
#include <string.h>
static uint16_t
mulib_uip_tcp_r16_ (const uint8_t *p)
{
  return (uint16_t)(p[0] << 8 | p[1]);
}
static uint32_t
mulib_uip_tcp_r32_ (const uint8_t *p)
{
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8
         | p[3];
}
static void
mulib_uip_tcp_w32_ (uint8_t *p, uint32_t v)
{
  for (int i = 0; i < 4; i++)
    p[i] = (uint8_t)(v >> (24 - 8 * i));
}
int
mulib_uip_tcp_parse (const uint8_t src[4], const uint8_t dst[4],
                     const void *packet, size_t n,
                     mulib_uip_tcp_segment_t *out)
{
  if (!src || !dst || !packet || !out || n < 20 || n > 65535)
    return -EINVAL;
  const uint8_t *b = packet;
  size_t h = (b[12] >> 4) * 4;
  if (h < 20 || h > n || mulib_uip_checksum_ipv4 (src, dst, 6, b, n))
    return -EBADMSG;
  mulib_uip_tcp_segment_t v = { 0 };
  v.source_port = mulib_uip_tcp_r16_ (b);
  v.dest_port = mulib_uip_tcp_r16_ (b + 2);
  v.sequence = mulib_uip_tcp_r32_ (b + 4);
  v.acknowledgment = mulib_uip_tcp_r32_ (b + 8);
  v.flags = b[13];
  v.window = mulib_uip_tcp_r16_ (b + 14);
  v.data = b + h;
  v.length = n - h;
  *out = v;
  return 0;
}
int
mulib_uip_tcp_pack (void *out, size_t cap, const uint8_t src[4],
                    const uint8_t dst[4], const mulib_uip_tcp_segment_t *s,
                    size_t *used)
{
  if (!out || !src || !dst || !s || !used || (!s->data && s->length)
      || s->length > 65515 || cap < 20 + s->length)
    return -EINVAL;
  uint8_t *b = out;
  memset (b, 0, 20);
  b[0] = (uint8_t)(s->source_port >> 8);
  b[1] = (uint8_t)s->source_port;
  b[2] = (uint8_t)(s->dest_port >> 8);
  b[3] = (uint8_t)s->dest_port;
  mulib_uip_tcp_w32_ (b + 4, s->sequence);
  mulib_uip_tcp_w32_ (b + 8, s->acknowledgment);
  b[12] = 0x50;
  b[13] = s->flags;
  b[14] = (uint8_t)(s->window >> 8);
  b[15] = (uint8_t)s->window;
  if (s->length)
    memcpy (b + 20, s->data, s->length);
  uint16_t sum = mulib_uip_checksum_ipv4 (src, dst, 6, b, 20 + s->length);
  b[16] = (uint8_t)(sum >> 8);
  b[17] = (uint8_t)sum;
  *used = 20 + s->length;
  return 0;
}
#endif
