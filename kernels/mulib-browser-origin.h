/** @file mulib-browser-origin.h
 * @brief Normalized HTTP(S) origin parsing and same-origin checks.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_ORIGIN in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_BROWSER_ORIGIN_H
#define MULIB_BROWSER_ORIGIN_H
#include "mulib-uri.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_origin
  {
    char scheme[8], host[256];
    uint16_t port;
  } mulib_browser_origin_t;
  /** @brief See manual/mulib-browser-origin.md for this operation's contract.
   */
  int mulib_browser_origin_parse (const char *url,
                                  mulib_browser_origin_t *out);
  /** @brief See manual/mulib-browser-origin.md for this operation's contract.
   */
  int mulib_browser_origin_equal (const mulib_browser_origin_t *a,
                                  const mulib_browser_origin_t *b);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_ORIGIN_H */
#ifdef MULIB_IMPL_BROWSER_ORIGIN
#ifndef MULIB_IMPL_BROWSER_ORIGIN_ONCE
#define MULIB_IMPL_BROWSER_ORIGIN_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_browser_origin_parse (const char *url, mulib_browser_origin_t *o)
{
  if (!url || !o || strlen (url) > 8192)
    return 0;
  mulib_uri_t u;
  if (!mulib_uri_parse (url, strlen (url), &u) || !u.has_authority
      || u.has_userinfo || !u.host.length)
    return 0;
  mulib_browser_origin_t z = { 0 };
  if (u.scheme.length < 4 || u.scheme.length > 5)
    return 0;
  for (size_t i = 0; i < u.scheme.length; i++)
    {
      unsigned char c = (unsigned char)u.scheme.data[i];
      z.scheme[i] = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
    }
  if (!strcmp (z.scheme, "https"))
    z.port = 443;
  else if (!strcmp (z.scheme, "http"))
    z.port = 80;
  else
    return 0;
  const char *h = u.host.data;
  size_t n = u.host.length;
  if (h[0] == '[')
    {
      mulib_inet6_t a;
      if (n < 3 || h[n - 1] != ']' || !mulib_inet6_parse (h + 1, n - 2, &a)
          || !mulib_inet6_format (&a, z.host, sizeof z.host))
        return 0;
    }
  else
    {
      if (n && h[n - 1] == '.')
        n--;
      if (!n || n > 253)
        return 0;
      size_t label = 0;
      for (size_t i = 0; i < n; i++)
        {
          unsigned char c = (unsigned char)h[i];
          if (c == '.')
            {
              if (!label || h[i - 1] == '-')
                return 0;
              label = 0;
            }
          else
            {
              if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                    || (c >= '0' && c <= '9') || c == '-')
                  || (!label && c == '-') || ++label > 63)
                return 0;
            }
          z.host[i] = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
        }
      if (!label || h[n - 1] == '-')
        return 0;
      int numeric = 1;
      for (size_t i = 0; i < n; i++)
        if ((h[i] < '0' || h[i] > '9') && h[i] != '.')
          numeric = 0;
      if (numeric)
        {
          mulib_inet4_t a;
          if (!mulib_inet4_parse (h, n, &a))
            return 0;
        }
      if (n >= 2 && z.host[0] == '0' && z.host[1] == 'x')
        return 0;
    }
  if (u.has_port)
    {
      unsigned v = 0;
      if (!u.port.length)
        return 0;
      for (size_t i = 0; i < u.port.length; i++)
        {
          unsigned char c = (unsigned char)u.port.data[i];
          if (c < '0' || c > '9' || v > (65535u - (c - '0')) / 10)
            return 0;
          v = v * 10 + c - '0';
        }
      if (!v)
        return 0;
      z.port = (uint16_t)v;
    }
  *o = z;
  return 1;
}
int
mulib_browser_origin_equal (const mulib_browser_origin_t *a,
                            const mulib_browser_origin_t *b)
{
  return a && b && a->port && b->port
         && memchr (a->scheme, 0, sizeof a->scheme)
         && memchr (b->scheme, 0, sizeof b->scheme)
         && memchr (a->host, 0, sizeof a->host)
         && memchr (b->host, 0, sizeof b->host) && a->host[0] && b->host[0]
         && a->port == b->port && !strcmp (a->scheme, b->scheme)
         && !strcmp (a->host, b->host);
}
#endif
#endif
