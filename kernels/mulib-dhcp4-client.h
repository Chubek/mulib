/** @file mulib-dhcp4-client.h
 * @brief Build DHCPv4 DISCOVER messages and decode DHCP message type options
 * C11, single-header, define MULIB_IMPL_DHCP4_CLIENT exactly once.
 */
#ifndef MULIB_DHCP4_CLIENT_H
#define MULIB_DHCP4_CLIENT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_dhcp4_client_discover (void *out, size_t cap, const uint8_t mac[6],
                                   uint32_t xid, size_t *written);
  int mulib_dhcp4_client_type (const void *packet, size_t size, uint8_t *type);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DHCP4_CLIENT)                                          \
    && !defined(MULIB_DHCP4_CLIENT_IMPLEMENTED)
#define MULIB_DHCP4_CLIENT_IMPLEMENTED
#include <errno.h>
#include <string.h>
int
mulib_dhcp4_client_discover (void *buf, size_t n, const uint8_t mac[6],
                             uint32_t xid, size_t *used)
{
  if (!buf || !mac || !used || n < 244)
    return -EINVAL;
  uint8_t *b = buf;
  memset (b, 0, 244);
  b[0] = 1;
  b[1] = 1;
  b[2] = 6;
  b[4] = (uint8_t)(xid >> 24);
  b[5] = (uint8_t)(xid >> 16);
  b[6] = (uint8_t)(xid >> 8);
  b[7] = (uint8_t)xid;
  b[10] = 0x80;
  memcpy (b + 28, mac, 6);
  b[236] = 99;
  b[237] = 130;
  b[238] = 83;
  b[239] = 99;
  b[240] = 53;
  b[241] = 1;
  b[242] = 1;
  b[243] = 255;
  *used = 244;
  return 0;
}
int
mulib_dhcp4_client_type (const void *packet, size_t n, uint8_t *type)
{
  if (!packet || !type || n < 240)
    return -EINVAL;
  const uint8_t *b = packet;
  if (b[236] != 99 || b[237] != 130 || b[238] != 83 || b[239] != 99)
    return -EBADMSG;
  size_t i = 240;
  while (i < n)
    {
      uint8_t t = b[i++];
      if (t == 255)
        break;
      if (!t)
        continue;
      if (i >= n)
        return -EBADMSG;
      uint8_t z = b[i++];
      if (z > n - i)
        return -EBADMSG;
      if (t == 53)
        {
          if (z != 1)
            return -EBADMSG;
          *type = b[i];
          return 0;
        }
      i += z;
    }
  return -ENOENT;
}
#endif
