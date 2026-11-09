/** @file mulib-charset.h
 * @brief Bounded charset parameter extraction from Content-Type headers.
 * C11 single-header implementation. Define MULIB_IMPL_CHARSET in one
 * translation unit.
 */
#ifndef MULIB_CHARSET_H
#define MULIB_CHARSET_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Extract the first charset parameter as a lowercase token. Quoted
   * tokens are accepted. 0 absent/malformed/too small, output unchanged.
   * Input/output must not overlap. Other quoted parameters are skipped. */
  int mulib_charset_extract (const char *value, char *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CHARSET) && !defined(MULIB_CHARSET_IMPLEMENTED)
#define MULIB_CHARSET_IMPLEMENTED
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
  mulib_charset_token_ (unsigned char c)
  {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
           || (c >= '0' && c <= '9')
           || (c && strchr ("!#$%&'*+-.^_`|~", c) != NULL);
  }
  int
  mulib_charset_extract (const char *s, char *out, size_t cap)
  {
    if (!s || !out)
      return 0;
    for (const char *p = s; *p; ++p)
      if (((unsigned char)*p < 32 && *p != '\t') || (unsigned char)*p == 127)
        return 0;
    while (*s && *s != ';')
      ++s;
    const char *found = NULL;
    size_t length = 0;
    while (*s)
      {
        ++s;
        while (*s == ' ' || *s == '\t')
          ++s;
        const char *key = s;
        while (mulib_charset_token_ ((unsigned char)*s))
          ++s;
        size_t kn = (size_t)(s - key);
        if (!kn)
          return 0;
        int charset = kn == 7;
        for (size_t i = 0; charset && i < 7; ++i)
          if ((key[i] >= 'A' && key[i] <= 'Z' ? key[i] + 32 : key[i])
              != "charset"[i])
            charset = 0;
        while (*s == ' ' || *s == '\t')
          ++s;
        if (*s != '=')
          return 0;
        ++s;
        while (*s == ' ' || *s == '\t')
          ++s;
        const char *value = s;
        size_t n;
        if (*s == '"')
          {
            value = ++s;
            while (*s && *s != '"')
              {
                if (*s == '\\')
                  {
                    ++s;
                    if (!*s)
                      return 0;
                  }
                ++s;
              }
            if (*s != '"')
              return 0;
            n = (size_t)(s - value);
            ++s;
          }
        else
          {
            while (mulib_charset_token_ ((unsigned char)*s))
              ++s;
            n = (size_t)(s - value);
            if (!n)
              return 0;
          }
        if (charset)
          {
            if (!n || found)
              return 0;
            for (size_t i = 0; i < n; ++i)
              if (!mulib_charset_token_ ((unsigned char)value[i]))
                return 0;
            found = value;
            length = n;
          }
        while (*s == ' ' || *s == '\t')
          ++s;
        if (*s && *s != ';')
          return 0;
      }
    if (!found || length >= cap)
      return 0;
    for (size_t i = 0; i < length; ++i)
      out[i] = found[i] >= 'A' && found[i] <= 'Z' ? (char)(found[i] + 32)
                                                  : found[i];
    out[length] = 0;
    return 1;
  }

#ifdef __cplusplus
}
#endif
#endif
