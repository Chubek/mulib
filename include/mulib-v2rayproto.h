/** @file mulib-v2rayproto.h
 * @brief VLESS version-zero request headers without addons or multiplexing.
 * Define MULIB_IMPL_V2RAYPROTO and MULIB_IMPL_SOCKS5 once. UUID is an
 * identifier, not encryption; use an authenticated transport. VMess is not
 * implemented.
 */
#ifndef MULIB_V2RAYPROTO_H
#define MULIB_V2RAYPROTO_H
#include "mulib-socks5.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_v2rayproto_request
  {
    unsigned char uuid[16];
    unsigned command; /* 1 TCP, 2 UDP */
    mulib_socks5_address_t address;
  } mulib_v2rayproto_request_t;
  /** @brief Decode header: 1 success, 0 incomplete, -1 malformed, -2
   * unsupported version/addons/command. Output unchanged on failure; used=0.
   */
  int mulib_v2rayproto_decode (const void *, size_t,
                               mulib_v2rayproto_request_t *, size_t *used);
  /** @brief Encode supported header, 1 success/0 failure; used=0 on failure.
   */
  int mulib_v2rayproto_encode (const mulib_v2rayproto_request_t *, void *,
                               size_t, size_t *used);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_V2RAYPROTO) && !defined(MULIB_V2RAYPROTO_IMPLEMENTED)
#define MULIB_V2RAYPROTO_IMPLEMENTED
#include <string.h>
int
mulib_v2rayproto_decode (const void *data, size_t n,
                         mulib_v2rayproto_request_t *out, size_t *used)
{
  const unsigned char *p = data;
  unsigned char tmp[259];
  size_t len, h, k;
  mulib_v2rayproto_request_t v;
  int r;
  if (used)
    *used = 0;
  if ((!p && n) || !out || !used)
    return -1;
  if (n && p[0])
    return -2;
  if (n < 18)
    return 0;
  if (p[17])
    return -2;
  if (n < 22)
    return 0;
  if (p[18] != 1 && p[18] != 2)
    return -2;
  if (p[21] == 1)
    {
      tmp[0] = 1;
      h = 1;
      len = 4;
    }
  else if (p[21] == 3)
    {
      tmp[0] = 4;
      h = 1;
      len = 16;
    }
  else if (p[21] == 2)
    {
      if (n < 23)
        return 0;
      tmp[0] = 3;
      tmp[1] = p[22];
      h = 2;
      len = p[22];
      if (!len)
        return -1;
    }
  else
    return -1;
  if (n < 21 + h + len)
    return 0;
  memcpy (tmp + h, p + 21 + h, len);
  tmp[h + len] = p[19];
  tmp[h + len + 1] = p[20];
  r = mulib_socks5_address_decode (tmp, h + len + 2, &v.address, &k);
  if (r != 1 || !v.address.port)
    return -1;
  memcpy (v.uuid, p + 1, 16);
  v.command = p[18];
  *out = v;
  *used = 21 + h + len;
  return 1;
}
int
mulib_v2rayproto_encode (const mulib_v2rayproto_request_t *v, void *data,
                         size_t cap, size_t *used)
{
  unsigned char *p = data, tmp[259];
  size_t k;
  if (used)
    *used = 0;
  if (!v || !p || !used || !v->address.port
      || (v->command != 1 && v->command != 2)
      || !mulib_socks5_address_encode (&v->address, tmp, sizeof (tmp), &k)
      || cap < k + 19)
    return 0;
  p[0] = 0;
  memcpy (p + 1, v->uuid, 16);
  p[17] = 0;
  p[18] = (unsigned char)v->command;
  p[19] = tmp[k - 2];
  p[20] = tmp[k - 1];
  p[21] = tmp[0] == 1 ? 1 : tmp[0] == 4 ? 3 : 2;
  memcpy (p + 22, tmp + 1, k - 3);
  *used = k + 19;
  return 1;
}
#endif
