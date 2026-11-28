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

/* Normalize the host to the tuple representation (unbracketed for IPv6). */
static int
mulib_browser_origin_host_ (const char *h, size_t n, int ipv6, char *out,
                            size_t cap)
{
  if (ipv6)
    {
      mulib_inet6_t a;
      return mulib_inet6_parse (h, n, &a)
             && mulib_inet6_format (&a, out, cap);
    }
  if (n && h[n - 1] == '.')
    n--;
  if (!n || n > 253 || n >= cap)
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
      else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                 || (c >= '0' && c <= '9') || c == '-')
               || (!label && c == '-') || ++label > 63)
        return 0;
      out[i] = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
    }
  if (!label || h[n - 1] == '-')
    return 0;
  /* Do not accept a DNS tuple when the last label looks like an IPv4
     component. Require strict dotted decimal for such hosts. */
  const char *last = out + n - label;
  int numeric = 1, hex = label >= 2 && last[0] == '0' && last[1] == 'x';
  for (size_t i = 0; i < label; i++)
    if (last[i] < '0' || last[i] > '9')
      numeric = 0;
  for (size_t i = 2; hex && i < label; i++)
    if ((last[i] < '0' || last[i] > '9') && (last[i] < 'a' || last[i] > 'f'))
      hex = 0;
  if (numeric || hex)
    {
      mulib_inet4_t a;
      if (!mulib_inet4_parse (out, n, &a))
        return 0;
    }
  /* Preserve the existing exclusion of a leading hex-looking label. */
  if (n >= 2 && out[0] == '0' && out[1] == 'x')
    return 0;
  out[n] = '\0';
  return 1;
}

int
mulib_browser_origin_parse (const char *url, mulib_browser_origin_t *o)
{
  if (!url || !o)
    return 0;
  size_t length = 0;
  while (length <= 8192 && url[length])
    length++;
  mulib_uri_t u;
  if (length > 8192 || !mulib_uri_parse (url, length, &u) || !u.has_authority
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
  int ipv6 = h[0] == '[';
  if (ipv6)
    {
      if (n < 3 || h[n - 1] != ']')
        return 0;
      h++;
      n -= 2;
    }
  if (!mulib_browser_origin_host_ (h, n, ipv6, z.host, sizeof z.host))
    return 0;
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
  if (!a || !b || !a->port || a->port != b->port
      || !memchr (a->scheme, 0, sizeof a->scheme)
      || !memchr (b->scheme, 0, sizeof b->scheme)
      || !memchr (a->host, 0, sizeof a->host)
      || !memchr (b->host, 0, sizeof b->host)
      || (strcmp (a->scheme, "https") && strcmp (a->scheme, "http"))
      || strcmp (a->scheme, b->scheme) || strcmp (a->host, b->host))
    return 0;
  char canonical[sizeof a->host];
  return mulib_browser_origin_host_ (a->host, strlen (a->host),
                                     strchr (a->host, ':') != NULL, canonical,
                                     sizeof canonical)
         && !strcmp (a->host, canonical);
}
#endif
#endif
