/** @file mulib-packetfiter.h
 * @brief Bounds-checked IP packet metadata for filtering (historical
 * spelling). Define MULIB_IMPL_PACKETFITER once. No checksum verification or
 * reassembly.
 */
#ifndef MULIB_PACKETFITER_H
#define MULIB_PACKETFITER_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_packetfiter_view
  {
    unsigned family, protocol;
    unsigned char source[16], destination[16];
    uint16_t source_port, destination_port;
    int fragmented, has_ports;
    size_t length, payload_offset;
  } mulib_packetfiter_view_t;
  /** @brief Parse an IP packet, permitting trailing link padding. IPv4 options
   * are skipped; IPv6 extension headers are explicitly unsupported (-2).
   * 1 success, 0 truncated/malformed, -1 invalid arguments, -2 unsupported.
   * Fragmented IPv4 never exposes ports; contradictory DF, empty fragments,
   * misaligned nonfinal fragments and impossible reassembly sizes are rejected.
   * Output unchanged on failure. */
  int mulib_packetfiter_parse (const void *, size_t,
                               mulib_packetfiter_view_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PACKETFITER) && !defined(MULIB_PACKETFITER_IMPLEMENTED)
#define MULIB_PACKETFITER_IMPLEMENTED
#include <string.h>
static unsigned
mulib_packetfiter_u16_ (const unsigned char *p)
{
  return (unsigned)p[0] * 256u + p[1];
}
int
mulib_packetfiter_parse (const void *data, size_t n,
                         mulib_packetfiter_view_t *out)
{
  const unsigned char *p = data;
  mulib_packetfiter_view_t v;
  size_t h, total, left;
  if ((!p && n) || !out)
    return -1;
  if (!n)
    return 0;
  memset (&v, 0, sizeof (v));
  if ((p[0] >> 4) == 4)
    {
      unsigned fragment;
      if (n < 20)
        return 0;
      h = (p[0] & 15u) * 4u;
      total = mulib_packetfiter_u16_ (p + 2);
      if (h < 20 || h > total || total > n || (p[6] & 0x80))
        return 0;
      fragment = mulib_packetfiter_u16_ (p + 6);
      if ((fragment & 0x3fff)
          && ((fragment & 0x4000) || total == h
              || ((fragment & 0x2000) && (total - h) % 8)
              || (size_t)(fragment & 0x1fff) * 8 + total - h > 65535 - 20))
        return 0;
      v.family = 4;
      v.protocol = p[9];
      v.fragmented = (fragment & 0x3fff) != 0;
      memcpy (v.source, p + 12, 4);
      memcpy (v.destination, p + 16, 4);
    }
  else if ((p[0] >> 4) == 6)
    {
      if (n < 40)
        return 0;
      h = 40;
      total = 40 + mulib_packetfiter_u16_ (p + 4);
      if (total > n)
        return 0;
      v.family = 6;
      v.protocol = p[6];
      if (v.protocol == 0 || v.protocol == 43 || v.protocol == 44
          || v.protocol == 50 || v.protocol == 51 || v.protocol == 60
          || v.protocol == 135 || v.protocol == 139 || v.protocol == 140
          || v.protocol == 253 || v.protocol == 254)
        return -2;
      memcpy (v.source, p + 8, 16);
      memcpy (v.destination, p + 24, 16);
    }
  else
    return -2;
  left = total - h;
  if (!v.fragmented && (v.protocol == 6 || v.protocol == 17))
    {
      if (v.protocol == 6)
        {
          size_t th;
          if (left < 20)
            return 0;
          th = (p[h + 12] >> 4) * 4u;
          if (th < 20 || th > left)
            return 0;
          v.payload_offset = h + th;
        }
      else
        {
          unsigned u;
          if (left < 8)
            return 0;
          u = mulib_packetfiter_u16_ (p + h + 4);
          if (u < 8 || u != left)
            return 0;
          v.payload_offset = h + 8;
        }
      v.source_port = (uint16_t)mulib_packetfiter_u16_ (p + h);
      v.destination_port = (uint16_t)mulib_packetfiter_u16_ (p + h + 2);
      v.has_ports = 1;
    }
  else
    v.payload_offset = h;
  v.length = total;
  *out = v;
  return 1;
}
#endif
