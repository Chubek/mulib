/** @file mulib-cookie.h
 * @brief HTTP Set-Cookie name/value validation and extraction.
 * C11 single-header implementation. Define MULIB_IMPL_COOKIE in one
 * translation unit.
 */
#ifndef MULIB_COOKIE_H
#define MULIB_COOKIE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Copy an RFC 6265 token/cookie-octet pair (quotes preserved).
   * Attributes are ignored, but controls anywhere are rejected. Returns 0 on
   * invalid input or capacity failure without writing outputs. No overlap. */
  int mulib_cookie_pair (const char *header, char *name, size_t nc,
                         char *value, size_t vc);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_COOKIE) && !defined(MULIB_COOKIE_IMPLEMENTED)
#define MULIB_COOKIE_IMPLEMENTED
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
  mulib_cookie_pair (const char *s, char *n, size_t nc, char *v, size_t vc)
  {
    if (!s || !n || !v)
      return 0;
    for (const char *p = s; *p; ++p)
      if ((unsigned char)*p < 32 || (unsigned char)*p == 127)
        return 0;
    const char *e = strchr (s, ';');
    if (!e)
      e = s + strlen (s);
    const char *q = (const char *)memchr (s, '=', (size_t)(e - s));
    if (!q || q == s)
      return 0;
    size_t a = (size_t)(q - s), b = (size_t)(e - q - 1);
    if (a >= nc || b >= vc)
      return 0;
    for (const char *p = s; p < q; ++p)
      {
        unsigned char c = (unsigned char)*p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
              || (c >= '0' && c <= '9') || strchr ("!#$%&'*+-.^_`|~", c)))
          return 0;
      }
    const char *start = q + 1, *end = e;
    if (start < end && *start == '"')
      {
        if (end - start < 2 || end[-1] != '"')
          return 0;
        ++start;
        --end;
      }
    for (const char *p = start; p < end; ++p)
      {
        unsigned char c = (unsigned char)*p;
        if (!(c == 0x21 || (c >= 0x23 && c <= 0x2b) || (c >= 0x2d && c <= 0x3a)
              || (c >= 0x3c && c <= 0x5b) || (c >= 0x5d && c <= 0x7e)))
          return 0;
      }
    memcpy (n, s, a);
    n[a] = 0;
    memcpy (v, q + 1, b);
    v[b] = 0;
    return 1;
  }

#ifdef __cplusplus
}
#endif
#endif
