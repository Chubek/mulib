/** @file mulib-net-profile.h
 * @brief Parse a bounded flat INI-style network profile with interface, IPv4
 * address and gateway fields C11, single-header, define MULIB_IMPL_NET_PROFILE
 * exactly once.
 */
#ifndef MULIB_NET_PROFILE_H
#define MULIB_NET_PROFILE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_net_profile
  {
    char interface[32], address[48], gateway[48];
    unsigned enabled;
  } mulib_net_profile_t;
  int mulib_net_profile_parse (const char *text, size_t length,
                               mulib_net_profile_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NET_PROFILE) && !defined(MULIB_NET_PROFILE_IMPLEMENTED)
#define MULIB_NET_PROFILE_IMPLEMENTED
#include <errno.h>
#include <string.h>
static int
mulib_net_profile_set_ (char *dest, size_t cap, const char *p, size_t n)
{
  if (n >= cap)
    return -ENOSPC;
  memcpy (dest, p, n);
  dest[n] = 0;
  return 0;
}
int
mulib_net_profile_parse (const char *text, size_t n, mulib_net_profile_t *out)
{
  if (!text || !out || n > 4096 || memchr (text, 0, n))
    return -EINVAL;
  mulib_net_profile_t v = { 0 };
  v.enabled = 1;
  size_t start = 0;
  for (size_t i = 0; i <= n; i++)
    if (i == n || text[i] == '\n')
      {
        size_t e = i;
        while (e > start && (text[e - 1] == '\r' || text[e - 1] == ' '))
          e--;
        if (e > start && text[start] != '#' && text[start] != ';')
          {
            size_t k = start;
            while (k < e && text[k] != '=')
              k++;
            if (k == e)
              return -EBADMSG;
            const char *p = text + k + 1;
            size_t len = e - k - 1;
            int r = 0;
            if (k - start == 9 && !memcmp (text + start, "interface", 9))
              r = mulib_net_profile_set_ (v.interface, sizeof v.interface, p,
                                          len);
            else if (k - start == 7 && !memcmp (text + start, "address", 7))
              r = mulib_net_profile_set_ (v.address, sizeof v.address, p, len);
            else if (k - start == 7 && !memcmp (text + start, "gateway", 7))
              r = mulib_net_profile_set_ (v.gateway, sizeof v.gateway, p, len);
            else if (k - start == 7 && !memcmp (text + start, "enabled", 7))
              {
                if (len != 1 || (p[0] != '0' && p[0] != '1'))
                  return -EBADMSG;
                v.enabled = (unsigned)(p[0] == '1');
              }
            else
              return -EINVAL;
            if (r)
              return r;
          }
        start = i + 1;
      }
  if (!v.interface[0])
    return -EINVAL;
  *out = v;
  return 0;
}
#endif
