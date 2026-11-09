/** @file mulib-uip-dns.h
 * @brief Build DNS A queries and decode one uncompressed or compressed A
 * answer C11, single-header, define MULIB_IMPL_UIP_DNS exactly once.
 */
#ifndef MULIB_UIP_DNS_H
#define MULIB_UIP_DNS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_uip_dns_query_a (const char *domain, uint16_t id, void *out,
                             size_t capacity, size_t *written);
  int mulib_uip_dns_first_a (const void *reply, size_t n, uint16_t expected_id,
                             uint8_t ip[4]);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UIP_DNS) && !defined(MULIB_UIP_DNS_IMPLEMENTED)
#define MULIB_UIP_DNS_IMPLEMENTED
#include <errno.h>
#include <string.h>
static int
mulib_uip_dns_skip_ (const uint8_t *b, size_t n, size_t *i)
{
  size_t steps = 0, pos = *i, expanded = 1;
  int jumped = 0;
  while (pos < n && steps++ < 128)
    {
      size_t at = pos;
      uint8_t z = b[pos++];
      if (!z)
        {
          if (!jumped)
            *i = pos;
          return 0;
        }
      if ((z & 0xc0) == 0xc0)
        {
          if (pos >= n)
            return -EBADMSG;
          size_t target = ((size_t)(z & 63u) << 8) | b[pos++];
          if (target < 12 || target >= at)
            return -EBADMSG;
          if (!jumped)
            *i = pos;
          jumped = 1;
          pos = target;
          continue;
        }
      if (z & 0xc0 || z > 63 || z > n - pos || expanded + z + 1 > 255)
        return -EBADMSG;
      expanded += z + 1;
      pos += z;
    }
  return -EBADMSG;
}
int
mulib_uip_dns_query_a (const char *domain, uint16_t id, void *out, size_t cap,
                       size_t *used)
{
  if (!domain || !out || !used || cap < 17 || strlen (domain) > 253)
    return -EINVAL;
  uint8_t *b = out;
  memset (b, 0, cap < 12 ? cap : 12);
  b[0] = (uint8_t)(id >> 8);
  b[1] = (uint8_t)id;
  b[2] = 1;
  b[5] = 1;
  size_t j = 12;
  const char *start = domain;
  for (const char *p = domain;; p++)
    if (*p == '.' || !*p)
      {
        size_t len = (size_t)(p - start);
        if (!len || len > 63 || j + len + 6 > cap)
          return -EINVAL;
        b[j++] = (uint8_t)len;
        memcpy (b + j, start, len);
        j += len;
        if (!*p)
          break;
        start = p + 1;
      }
  b[j++] = 0;
  b[j++] = 0;
  b[j++] = 1;
  b[j++] = 0;
  b[j++] = 1;
  *used = j;
  return 0;
}
int
mulib_uip_dns_first_a (const void *p, size_t n, uint16_t id, uint8_t ip[4])
{
  if (!p || !ip || n < 12)
    return -EINVAL;
  const uint8_t *b = p;
  if ((uint16_t)(b[0] << 8 | b[1]) != id || !(b[2] & 0x80) || (b[2] & 0x7a)
      || (b[3] & 15))
    return -EBADMSG;
  unsigned q = (unsigned)b[4] << 8 | b[5],
           answers = (unsigned)b[6] << 8 | b[7];
  size_t i = 12;
  if (q > 128 || answers > 1024)
    return -EBADMSG;
  for (unsigned k = 0; k < q; k++)
    {
      int r = mulib_uip_dns_skip_ (b, n, &i);
      if (r || n - i < 4)
        return -EBADMSG;
      i += 4;
    }
  for (unsigned k = 0; k < answers; k++)
    {
      int r = mulib_uip_dns_skip_ (b, n, &i);
      if (r || n - i < 10)
        return -EBADMSG;
      uint16_t type = (uint16_t)(b[i] << 8 | b[i + 1]);
      uint16_t cls = (uint16_t)(b[i + 2] << 8 | b[i + 3]);
      size_t len = (size_t)b[i + 8] << 8 | b[i + 9];
      i += 10;
      if (len > n - i)
        return -EBADMSG;
      if (type == 1 && cls == 1 && len == 4)
        {
          memcpy (ip, b + i, 4);
          return 0;
        }
      i += len;
    }
  return -ENOENT;
}
#endif
