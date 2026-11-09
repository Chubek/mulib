/** @file mulib-domserialize.h
 * @brief HTML text escaping into caller-owned output buffer.
 * C11 single-header implementation. Define MULIB_IMPL_DOMSERIALIZE in one
 * translation unit.
 */
#ifndef MULIB_DOMSERIALIZE_H
#define MULIB_DOMSERIALIZE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Escape &, <, > and quotes; returns output bytes excluding NUL.
   * 0 empty/error (empty success writes NUL). Capacity failure leaves output
   * unchanged. No overlap, Unicode validation, DOM traversal or URL
   * sanitization. */
  size_t mulib_domserialize_escape (const char *text, char *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DOMSERIALIZE)                                          \
    && !defined(MULIB_DOMSERIALIZE_IMPLEMENTED)
#define MULIB_DOMSERIALIZE_IMPLEMENTED
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif

  static const char *
  mulib_domserialize_rep_ (char c)
  {
    switch (c)
      {
      case '&':
        return "&amp;";
      case '<':
        return "&lt;";
      case '>':
        return "&gt;";
      case '"':
        return "&quot;";
      case '\'':
        return "&#39;";
      default:
        return NULL;
      }
  }
  size_t
  mulib_domserialize_escape (const char *s, char *o, size_t cap)
  {
    if (!s || !o || !cap)
      return 0;
    size_t k = 0;
    for (const char *p = s; *p; ++p)
      {
        const char *rep = mulib_domserialize_rep_ (*p);
        size_t n = rep ? strlen (rep) : 1;
        if (n >= cap - k)
          return 0;
        k += n;
      }
    k = 0;
    for (; *s; ++s)
      {
        const char *rep = mulib_domserialize_rep_ (*s);
        size_t n = rep ? strlen (rep) : 1;
        if (rep)
          memcpy (o + k, rep, n);
        else
          o[k] = *s;
        k += n;
      }
    o[k] = 0;
    return k;
  }

#ifdef __cplusplus
}
#endif
#endif
