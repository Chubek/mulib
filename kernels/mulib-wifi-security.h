/** @file mulib-wifi-security.h
 * @brief Strict bounded WPA2 RSN information element parsing
 * C11, single-header, define MULIB_IMPL_WIFI_SECURITY exactly once.
 */
#ifndef MULIB_WIFI_SECURITY_H
#define MULIB_WIFI_SECURITY_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_wifi_security_rsn
  {
    uint16_t version;
    uint16_t pairwise_count, akm_count;
    uint8_t group_suite[4];
  } mulib_wifi_security_rsn_t;
  int mulib_wifi_security_rsn_parse (const void *ie, size_t len,
                                     mulib_wifi_security_rsn_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WIFI_SECURITY)                                         \
    && !defined(MULIB_WIFI_SECURITY_IMPLEMENTED)
#define MULIB_WIFI_SECURITY_IMPLEMENTED
#include <errno.h>
#include <string.h>
int
mulib_wifi_security_rsn_parse (const void *p, size_t n,
                               mulib_wifi_security_rsn_t *out)
{
  if (!p || !out || n < 2)
    return -EINVAL;
  const uint8_t *b = p;
  if (b[0] != 48 || b[1] != n - 2 || n < 2 + 2 + 4 + 2 + 4 + 2 + 4)
    return -EBADMSG;
  size_t off = 2;
  mulib_wifi_security_rsn_t v = { 0 };
  v.version = b[off] | (b[off + 1] << 8);
  off += 2;
  if (v.version != 1)
    return -EPROTONOSUPPORT;
  memcpy (v.group_suite, b + off, 4);
  off += 4;
  v.pairwise_count = (uint16_t)(b[off] | b[off + 1] << 8);
  off += 2;
  if (!v.pairwise_count || v.pairwise_count > (n - off) / 4)
    return -EBADMSG;
  off += 4u * v.pairwise_count;
  if (n - off < 2)
    return -EBADMSG;
  v.akm_count = (uint16_t)(b[off] | b[off + 1] << 8);
  off += 2;
  if (!v.akm_count || v.akm_count > (n - off) / 4)
    return -EBADMSG;
  off += 4u * v.akm_count;
  if (off < n)
    {
      if (n - off < 2)
        return -EBADMSG;
      off += 2;
    }
  if (off < n)
    {
      if (n - off < 2)
        return -EBADMSG;
      size_t count = (size_t)b[off] | (size_t)b[off + 1] << 8;
      off += 2;
      if (count > (n - off) / 16)
        return -EBADMSG;
      off += count * 16;
    }
  if (off < n)
    {
      if (n - off != 4)
        return -EBADMSG;
      off += 4;
    }
  if (off != n)
    return -EBADMSG;
  *out = v;
  return 0;
}
#endif
