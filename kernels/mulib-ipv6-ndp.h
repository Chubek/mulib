/** @file mulib-ipv6-ndp.h
 * @brief Parse a bounded IPv6 Neighbor Solicitation or Advertisement header
 * C11, single-header, define MULIB_IMPL_IPV6_NDP exactly once.
 */
#ifndef MULIB_IPV6_NDP_H
#define MULIB_IPV6_NDP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_ipv6_ndp
  {
    uint8_t type, code;
    uint32_t flags;
    uint8_t target[16];
  } mulib_ipv6_ndp_t;
  int mulib_ipv6_ndp_parse (const void *icmp6, size_t length,
                            mulib_ipv6_ndp_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_IPV6_NDP) && !defined(MULIB_IPV6_NDP_IMPLEMENTED)
#define MULIB_IPV6_NDP_IMPLEMENTED
#include <errno.h>
#include <string.h>
int
mulib_ipv6_ndp_parse (const void *p, size_t n, mulib_ipv6_ndp_t *out)
{
  if (!p || !out || n < 24)
    return -EINVAL;
  const uint8_t *b = p;
  if ((b[0] != 135 && b[0] != 136) || b[1])
    return -EBADMSG;
  mulib_ipv6_ndp_t v = { 0 };
  v.type = b[0];
  v.code = b[1];
  v.flags = (uint32_t)b[4] << 24 | (uint32_t)b[5] << 16 | (uint32_t)b[6] << 8
            | b[7];
  memcpy (v.target, b + 8, 16);
  *out = v;
  return 0;
}
#endif
