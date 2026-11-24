/** @file mulib-netpolicy.h
 * @brief URL scheme and obvious loopback/private-host denial policy.
 * C11 single-header implementation. Define MULIB_IMPL_NETPOLICY in one
 * translation unit.
 */
#ifndef MULIB_NETPOLICY_H
#define MULIB_NETPOLICY_H
#include "mulib-urlcanon.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Conservative lexical policy: HTTP(S) DNS names or canonical public
   * dotted IPv4 only. Rejects IPv6, userinfo, local/single-label names and
   * numeric aliases. Requires URLCANON implementation. NOT a DNS/redirect/SSRF
   * defense: callers must validate resolved addresses and every redirect
   * separately. */
  int mulib_netpolicy_public_url (const char *url);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NETPOLICY) && !defined(MULIB_NETPOLICY_IMPLEMENTED)
#define MULIB_NETPOLICY_IMPLEMENTED
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif

  int
  mulib_netpolicy_public_url (const char *s)
  {
    char normalized[8194], host[254];
    if (!mulib_urlcanon_normalize (s, normalized, sizeof normalized))
      return 0;
    const char *p = strstr (normalized, "://") + 3;
    size_t n = 0;
    while (p[n] && p[n] != '/' && p[n] != ':' && p[n] != '?' && p[n] != '#')
      {
        if (n == sizeof host - 1)
          return 0;
        host[n] = p[n];
        ++n;
      }
    if (!n || host[0] == '[')
      return 0;
    if (host[n - 1] == '.')
      --n;
    host[n] = 0;
    const char *last = strrchr (host, '.');
    if (!last)
      return 0;
    if (!strcmp (last, ".localhost") || !strcmp (last, ".local")
        || !strcmp (last, ".internal") || !strcmp (last, ".home")
        || !strcmp (last, ".lan"))
      return 0;
    unsigned octets[4] = { 0 };
    size_t part = 0, digits = 0;
    int numeric = 1;
    for (size_t i = 0; i <= n; ++i)
      {
        if (host[i] >= '0' && host[i] <= '9')
          {
            if (part >= 4 || digits >= 3 || (digits && octets[part] == 0))
              {
                numeric = 0;
                break;
              }
            octets[part] = octets[part] * 10 + (unsigned)(host[i] - '0');
            ++digits;
            if (octets[part] > 255)
              {
                numeric = 0;
                break;
              }
          }
        else if (host[i] == '.' || !host[i])
          {
            if (!digits || part >= 4)
              {
                numeric = 0;
                break;
              }
            ++part;
            digits = 0;
          }
        else
          {
            numeric = 0;
            break;
          }
      }
    if (numeric && part == 4)
      {
        unsigned a = octets[0], b = octets[1], c = octets[2];
        if (a == 0 || a == 10 || a == 127 || a >= 224 || (a == 169 && b == 254)
            || (a == 192 && b == 168) || (a == 172 && b >= 16 && b <= 31)
            || (a == 100 && b >= 64 && b <= 127)
            || (a == 192 && b == 0 && (c == 0 || c == 2))
            || (a == 198 && (b == 18 || b == 19))
            || (a == 198 && b == 51 && c == 100)
            || (a == 203 && b == 0 && c == 113))
          return 0;
        return 1;
      }
    /* Browser IPv4 parsers also accept shortened, octal and hexadecimal forms.
     */
    ++last;
    if ((*last >= '0' && *last <= '9'))
      return 0;
    return 1;
  }

#ifdef __cplusplus
}
#endif
#endif
