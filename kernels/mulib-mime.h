/** @file mulib-mime.h
 * @brief Filename MIME lookup and bounded Content-Type normalization.
 * C11 single-header implementation. Define MULIB_IMPL_MIME in one translation
 * unit.
 */
#ifndef MULIB_MIME_H
#define MULIB_MIME_H
#include "mulib-seurity.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Static MIME string for a filename, or application/octet-stream. */
  const char *mulib_mime_type (const char *filename);
  /** @brief Copy lowercase HTTP token/token, excluding parameters. 1 success,
   * 0 invalid or insufficient space; output unchanged on failure. No overlap.
   */
  int mulib_mime_content_type (const char *header, char *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MIME) && !defined(MULIB_MIME_IMPLEMENTED)
#define MULIB_MIME_IMPLEMENTED
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif

  const char *
  mulib_mime_type (const char *s)
  {
    static const char *const types[][2]
        = { { "html", "text/html; charset=utf-8" },
            { "htm", "text/html; charset=utf-8" },
            { "css", "text/css; charset=utf-8" },
            { "js", "text/javascript; charset=utf-8" },
            { "json", "application/json" },
            { "txt", "text/plain; charset=utf-8" },
            { "svg", "image/svg+xml" },
            { "png", "image/png" },
            { "jpg", "image/jpeg" },
            { "jpeg", "image/jpeg" },
            { "gif", "image/gif" },
            { "ico", "image/x-icon" },
            { "pdf", "application/pdf" },
            { "wasm", "application/wasm" },
            { "xml", "application/xml" } };
    const char *dot = NULL;
    if (s)
      for (; *s; ++s)
        {
          if (*s == '/' || *s == '\\')
            dot = NULL;
          if (*s == '.')
            dot = s + 1;
        }
    if (dot)
      for (size_t i = 0; i < sizeof types / sizeof types[0]; ++i)
        if (mulib_security_ascii_equal (dot, types[i][0]))
          return types[i][1];
    return "application/octet-stream";
  }
  static int
  mulib_mime_token_ (unsigned char c)
  {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
           || (c >= '0' && c <= '9')
           || (c && strchr ("!#$%&'*+-.^_`|~", c) != NULL);
  }
  int
  mulib_mime_content_type (const char *s, char *o, size_t cap)
  {
    if (!s || !o)
      return 0;
    for (const char *p = s; *p; ++p)
      if (((unsigned char)*p < 32 && *p != '\t') || (unsigned char)*p == 127)
        return 0;
    while (*s == ' ' || *s == '\t')
      ++s;
    const char *a = s;
    while (mulib_mime_token_ ((unsigned char)*s))
      ++s;
    if (s == a || *s != '/')
      return 0;
    const char *b = ++s;
    while (mulib_mime_token_ ((unsigned char)*s))
      ++s;
    if (s == b)
      return 0;
    size_t n = (size_t)(s - a);
    while (*s == ' ' || *s == '\t')
      ++s;
    if ((*s && *s != ';') || n >= cap)
      return 0;
    for (size_t i = 0; i < n; ++i)
      o[i] = a[i] >= 'A' && a[i] <= 'Z' ? (char)(a[i] + 32) : a[i];
    o[n] = 0;
    return 1;
  }

#ifdef __cplusplus
}
#endif
#endif
