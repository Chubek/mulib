/** @file mulib-socks5.h
 * @brief RFC 1928 method negotiation, address and CONNECT/BIND/UDP codecs.
 * Define MULIB_IMPL_SOCKS5 once. No socket ownership or authentication engine.
 */
#ifndef MULIB_SOCKS5_H
#define MULIB_SOCKS5_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_socks5_address
  {
    unsigned type; /* 1 IPv4, 3 domain, 4 IPv6 */
    unsigned char host[255];
    size_t length;
    uint16_t port;
  } mulib_socks5_address_t;
  /** @brief Decode/encode address fields. Decode returns 1 success, 0 need
   * more, -1 malformed. Encode returns 1/0. used is zero on failure.
   * Domain is 1..255 bytes, no NUL/control bytes, no DNS interpretation. */
  int mulib_socks5_address_decode (const void *, size_t,
                                   mulib_socks5_address_t *, size_t *used);
  int mulib_socks5_address_encode (const mulib_socks5_address_t *, void *,
                                   size_t, size_t *used);
  /** @brief Parse client greeting, select first server-preferred offered
   * method. Output is [5,method] or [5,255] for no match; 1/0/-1 as decode
   * above. */
  int mulib_socks5_negotiate (const void *, size_t,
                              const unsigned char *preferred, size_t count,
                              unsigned char reply[2], size_t *used);
  /** @brief Decode request/reply header. reply=0 validates command 1..3;
   * reply=1 validates reply code 0..8. Reserved byte must be zero. */
  int mulib_socks5_decode (const void *, size_t, int reply, unsigned *code,
                           mulib_socks5_address_t *, size_t *used);
  /** @brief Encode a request/reply header. Nonoverlapping source/output. */
  int mulib_socks5_encode (unsigned code, int reply,
                           const mulib_socks5_address_t *, void *, size_t,
                           size_t *used);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SOCKS5) && !defined(MULIB_SOCKS5_IMPLEMENTED)
#define MULIB_SOCKS5_IMPLEMENTED
#include <string.h>
static int
mulib_socks5_valid_ (const mulib_socks5_address_t *a)
{
  if (!a)
    return 0;
  if (a->type == 1)
    return a->length == 4;
  if (a->type == 4)
    return a->length == 16;
  if (a->type != 3 || !a->length || a->length > 255)
    return 0;
  for (size_t i = 0; i < a->length; ++i)
    if (a->host[i] <= 32 || a->host[i] == 127)
      return 0;
  return 1;
}
int
mulib_socks5_address_decode (const void *data, size_t n,
                             mulib_socks5_address_t *out, size_t *used)
{
  const unsigned char *p = data;
  mulib_socks5_address_t a;
  size_t h = 1, len;
  if (used)
    *used = 0;
  if ((!p && n) || !out || !used)
    return -1;
  if (!n)
    return 0;
  memset (&a, 0, sizeof (a));
  a.type = p[0];
  if (p[0] == 1)
    len = 4;
  else if (p[0] == 4)
    len = 16;
  else if (p[0] == 3)
    {
      if (n < 2)
        return 0;
      h = 2;
      len = p[1];
      if (!len)
        return -1;
    }
  else
    return -1;
  if (n < h + len + 2)
    return 0;
  a.length = len;
  memcpy (a.host, p + h, len);
  a.port = (uint16_t)((unsigned)p[h + len] * 256 + p[h + len + 1]);
  if (!mulib_socks5_valid_ (&a))
    return -1;
  *out = a;
  *used = h + len + 2;
  return 1;
}
int
mulib_socks5_address_encode (const mulib_socks5_address_t *a, void *data,
                             size_t cap, size_t *used)
{
  unsigned char *p = data;
  size_t h;
  if (used)
    *used = 0;
  if (!p || !used || !mulib_socks5_valid_ (a))
    return 0;
  h = a->type == 3 ? 2 : 1;
  if (cap < h + a->length + 2)
    return 0;
  p[0] = (unsigned char)a->type;
  if (h == 2)
    p[1] = (unsigned char)a->length;
  memcpy (p + h, a->host, a->length);
  p[h + a->length] = (unsigned char)(a->port >> 8);
  p[h + a->length + 1] = (unsigned char)a->port;
  *used = h + a->length + 2;
  return 1;
}
int
mulib_socks5_negotiate (const void *data, size_t n, const unsigned char *pref,
                        size_t count, unsigned char reply[2], size_t *used)
{
  const unsigned char *p = data;
  unsigned method = 255;
  if (used)
    *used = 0;
  if ((!p && n) || (!pref && count) || !reply || !used || count > 255)
    return -1;
  if (n && p[0] != 5)
    return -1;
  if (n < 2)
    return 0;
  if (!p[1])
    return -1;
  if (n < (size_t)p[1] + 2)
    return 0;
  for (size_t i = 0; i < count && method == 255; ++i)
    for (unsigned j = 0; j < p[1]; ++j)
      if (pref[i] != 255 && pref[i] == p[2 + j])
        {
          method = pref[i];
          break;
        }
  reply[0] = 5;
  reply[1] = (unsigned char)method;
  *used = (size_t)p[1] + 2;
  return 1;
}
int
mulib_socks5_decode (const void *data, size_t n, int reply, unsigned *code,
                     mulib_socks5_address_t *a, size_t *used)
{
  const unsigned char *p = data;
  size_t k;
  int r;
  if (used)
    *used = 0;
  if ((!p && n) || !code || !a || !used || (reply != 0 && reply != 1))
    return -1;
  if (n && p[0] != 5)
    return -1;
  if (n < 3)
    return 0;
  if (p[2] || (reply ? p[1] > 8 : p[1] < 1 || p[1] > 3))
    return -1;
  r = mulib_socks5_address_decode (p + 3, n - 3, a, &k);
  if (r == 1)
    {
      *code = p[1];
      *used = k + 3;
    }
  return r;
}
int
mulib_socks5_encode (unsigned code, int reply, const mulib_socks5_address_t *a,
                     void *data, size_t n, size_t *used)
{
  unsigned char *p = data;
  size_t k;
  if (used)
    *used = 0;
  if (!p || !used || n < 3 || (reply != 0 && reply != 1)
      || (reply ? code > 8 : code < 1 || code > 3))
    return 0;
  if (!mulib_socks5_address_encode (a, p + 3, n - 3, &k))
    return 0;
  p[0] = 5;
  p[1] = (unsigned char)code;
  p[2] = 0;
  *used = k + 3;
  return 1;
}
#endif
