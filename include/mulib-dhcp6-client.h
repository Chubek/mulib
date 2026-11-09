/** @file mulib-dhcp6-client.h
 * @brief Build a DHCPv6 SOLICIT and parse a DHCPv6 option TLV
 * C11, single-header, define MULIB_IMPL_DHCP6_CLIENT exactly once.
 */
#ifndef MULIB_DHCP6_CLIENT_H
#define MULIB_DHCP6_CLIENT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_dhcp6_client_solicit (void *buf, size_t cap, uint32_t transaction,
                                  size_t *written);
  int mulib_dhcp6_client_option (const void *pkt, size_t size, uint16_t kind,
                                 const uint8_t **value, size_t *length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DHCP6_CLIENT)                                          \
    && !defined(MULIB_DHCP6_CLIENT_IMPLEMENTED)
#define MULIB_DHCP6_CLIENT_IMPLEMENTED
#include <errno.h>
int
mulib_dhcp6_client_solicit (void *buf, size_t n, uint32_t xid, size_t *written)
{
  if (!buf || !written || n < 4 || xid > 0xffffffu)
    return -EINVAL;
  uint8_t *b = buf;
  b[0] = 1;
  b[1] = (uint8_t)(xid >> 16);
  b[2] = (uint8_t)(xid >> 8);
  b[3] = (uint8_t)xid;
  *written = 4;
  return 0;
}
int
mulib_dhcp6_client_option (const void *pkt, size_t n, uint16_t kind,
                           const uint8_t **v, size_t *sz)
{
  if (!pkt || !v || !sz || n < 4)
    return -EINVAL;
  const uint8_t *b = pkt;
  size_t i = 4;
  while (i < n)
    {
      if (n - i < 4)
        return -EBADMSG;
      uint16_t t = (uint16_t)(b[i] << 8 | b[i + 1]);
      size_t len = (size_t)b[i + 2] << 8 | b[i + 3];
      i += 4;
      if (len > n - i)
        return -EBADMSG;
      if (t == kind)
        {
          *v = b + i;
          *sz = len;
          return 0;
        }
      i += len;
    }
  return -ENOENT;
}
#endif
