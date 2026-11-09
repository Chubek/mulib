/** @file mulib-ipv6-ra.h
 * @brief Parse ICMPv6 Router Advertisement header and a prefix-information
 * option C11, single-header, define MULIB_IMPL_IPV6_RA exactly once.
 */
#ifndef MULIB_IPV6_RA_H
#define MULIB_IPV6_RA_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_ipv6_ra
  {
    uint8_t hop_limit, flags;
    uint16_t lifetime;
  } mulib_ipv6_ra_t;
  int mulib_ipv6_ra_parse (const void *icmp6, size_t size,
                           mulib_ipv6_ra_t *out);
  int mulib_ipv6_ra_prefix (const void *icmp6, size_t size, uint8_t prefix[16],
                            unsigned *prefix_bits);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_IPV6_RA) && !defined(MULIB_IPV6_RA_IMPLEMENTED)
#define MULIB_IPV6_RA_IMPLEMENTED
#include <errno.h>
#include <string.h>
int
mulib_ipv6_ra_parse (const void *p, size_t n, mulib_ipv6_ra_t *out)
{
  if (!p || !out || n < 16)
    return -EINVAL;
  const uint8_t *b = p;
  if (b[0] != 134 || b[1])
    return -EBADMSG;
  mulib_ipv6_ra_t a = { b[4], b[5], (uint16_t)(b[6] << 8 | b[7]) };
  *out = a;
  return 0;
}
int
mulib_ipv6_ra_prefix (const void *p, size_t n, uint8_t prefix[16],
                      unsigned *bits)
{
  if (!prefix || !bits)
    return -EINVAL;
  mulib_ipv6_ra_t ra;
  int r = mulib_ipv6_ra_parse (p, n, &ra);
  if (r)
    return r;
  const uint8_t *b = p;
  size_t i = 16;
  while (i < n)
    {
      if (n - i < 2)
        return -EBADMSG;
      size_t len = (size_t)b[i + 1] * 8;
      if (len < 8 || len > n - i)
        return -EBADMSG;
      if (b[i] == 3)
        {
          if (len != 32 || b[i + 2] > 128)
            return -EBADMSG;
          memcpy (prefix, b + i + 16, 16);
          *bits = b[i + 2];
          return 0;
        }
      i += len;
    }
  return -ENOENT;
}
#endif
