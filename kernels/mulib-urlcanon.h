/** @file mulib-urlcanon.h
 * @brief Conservative URL normalization for HTTP(S) resource keys.
 * C11 single-header implementation. Define MULIB_IMPL_URLCANON in one
 * translation unit.
 */
#ifndef MULIB_URLCANON_H
#define MULIB_URLCANON_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Normalize HTTP(S) scheme/host, remove fragment/default port and
   * add an empty path's slash. ASCII DNS, dotted IPv4 and bracketed
   * hexadecimal IPv6 only; no userinfo, zone IDs, IDNA or dot-segment
   * resolution. At most 8192 input bytes. Returns 1, or 0 with output
   * unchanged. Exact in-place supported. */
  int mulib_urlcanon_normalize (const char *src, char *dst, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_URLCANON) && !defined(MULIB_URLCANON_IMPLEMENTED)
#define MULIB_URLCANON_IMPLEMENTED
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif

  static int
  mulib_urlcanon_hex_ (unsigned char c)
  {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')
           || (c >= 'A' && c <= 'F');
  }
  static int
  mulib_urlcanon_ipv6_ (const char *p, const char *end)
  {
    unsigned groups = 0;
    int compressed = 0;
    if (p == end)
      return 0;
    if (*p == ':')
      {
        if (end - p < 2 || p[1] != ':')
          return 0;
        compressed = 1;
        p += 2;
      }
    while (p < end)
      {
        unsigned digits = 0;
        while (p < end && mulib_urlcanon_hex_ ((unsigned char)*p))
          {
            ++p;
            ++digits;
          }
        if (!digits || digits > 4 || ++groups > 8)
          return 0;
        if (p == end)
          break;
        if (*p++ != ':' || p == end)
          return 0;
        if (*p == ':')
          {
            if (compressed)
              return 0;
            compressed = 1;
            ++p;
          }
      }
    return compressed ? groups < 8 : groups == 8;
  }
  int
  mulib_urlcanon_normalize (const char *s, char *o, size_t cap)
  {
    if (!s || !o)
      return 0;
    size_t len = 0;
    while (s[len])
      {
        if (len == 8192)
          return 0;
        ++len;
      }
    for (size_t i = 0; i < len; ++i)
      {
        unsigned char c = (unsigned char)s[i];
        if (c <= 32 || c >= 127 || c == '\\' || c == '"' || c == '<'
            || c == '>')
          return 0;
        if (c == '%'
            && (len - i < 3 || !mulib_urlcanon_hex_ ((unsigned char)s[i + 1])
                || !mulib_urlcanon_hex_ ((unsigned char)s[i + 2])))
          return 0;
      }
    const char *p = strstr (s, "://");
    if (!p)
      return 0;
    size_t scheme = (size_t)(p - s);
    if (scheme != 4 && scheme != 5)
      return 0;
    for (size_t i = 0; i < scheme; ++i)
      if ((s[i] >= 'A' && s[i] <= 'Z' ? s[i] + 32 : s[i]) != "https"[i])
        return 0;
    const char *host = p + 3, *path = host;
    while (*path && *path != '/' && *path != '?' && *path != '#')
      ++path;
    if (path == host)
      return 0;
    const char *port = NULL, *hend = path;
    if (*host == '[')
      {
        const char *close
            = (const char *)memchr (host, ']', (size_t)(path - host));
        if (!close || !mulib_urlcanon_ipv6_ (host + 1, close))
          return 0;
        hend = close + 1;
        if (hend < path)
          {
            if (*hend != ':')
              return 0;
            port = hend + 1;
          }
      }
    else
      {
        const char *colon
            = (const char *)memchr (host, ':', (size_t)(path - host));
        if (colon)
          {
            hend = colon;
            port = colon + 1;
          }
        size_t hn = (size_t)(hend - host), label = 0;
        if (!hn || hn > 253)
          return 0;
        for (const char *a = host; a < hend; ++a)
          {
            unsigned char c = (unsigned char)*a;
            if (c == '.')
              {
                if (!label || a[-1] == '-')
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
          }
        if (hend[-1] == '-')
          return 0;
      }
    unsigned number = 0;
    if (port)
      {
        if (port == path)
          return 0;
        for (const char *a = port; a < path; ++a)
          {
            if (*a < '0' || *a > '9' || number > 6553)
              return 0;
            number = number * 10 + (unsigned)(*a - '0');
            if (number > 65535)
              return 0;
          }
        if (!number)
          return 0;
      }
    const char *end = strchr (path, '#');
    if (!end)
      end = s + len;
    size_t hlen = (size_t)(path - host), tail = (size_t)(end - path);
    if (port && number == (scheme == 5 ? 443u : 80u))
      hlen = (size_t)(hend - host);
    int slash = path == end || *path == '?';
    size_t n = scheme + 3 + hlen + tail + (size_t)slash;
    if (n >= cap)
      return 0;
    /* Stage output to support exact in-place normalization, including
     * insertion. */
    char result[8194];
    size_t k = 0;
    for (size_t i = 0; i < scheme; ++i)
      result[k++] = (char)(s[i] >= 'A' && s[i] <= 'Z' ? s[i] + 32 : s[i]);
    memcpy (result + k, "://", 3);
    k += 3;
    for (size_t i = 0; i < hlen; ++i)
      result[k++]
          = (char)(host[i] >= 'A' && host[i] <= 'Z' ? host[i] + 32 : host[i]);
    if (slash)
      result[k++] = '/';
    memcpy (result + k, path, tail);
    k += tail;
    result[k] = 0;
    memmove (o, result, k + 1);
    return 1;
  }

#ifdef __cplusplus
}
#endif
#endif
