/** @file mulib-search-query.h
 * @brief Percent-encoded form query builder and Google/Bing URL construction.
 * C11 single-header implementation. Define MULIB_IMPL_SEARCH_QUERY in one
 * translation unit.
 */
#ifndef MULIB_SEARCH_QUERY_H
#define MULIB_SEARCH_QUERY_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Build a google/bing search URL; case-sensitive provider names.
   * ASCII unreserved bytes copied, space becomes '+', all other bytes percent
   * encoded (no UTF-8 validation). 1 success, 0 error; output unchanged. No
   * overlap. */
  int mulib_search_query_url (const char *provider, const char *query,
                              char *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SEARCH_QUERY)                                          \
    && !defined(MULIB_SEARCH_QUERY_IMPLEMENTED)
#define MULIB_SEARCH_QUERY_IMPLEMENTED
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
  mulib_search_query_plain_ (unsigned c)
  {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
           || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'
           || c == '~';
  }
  int
  mulib_search_query_url (const char *provider, const char *query, char *out,
                          size_t cap)
  {
    if (!provider || !query || !out)
      return 0;
    const char *base
        = !strcmp (provider, "google") ? "https://www.google.com/search?q="
          : !strcmp (provider, "bing") ? "https://www.bing.com/search?q="
                                       : NULL;
    if (!base)
      return 0;
    size_t k = strlen (base), n = k;
    if (n >= cap)
      return 0;
    for (const unsigned char *p = (const unsigned char *)query; *p; ++p)
      {
        size_t width = mulib_search_query_plain_ (*p) || *p == ' ' ? 1 : 3;
        if (width >= cap - n)
          return 0;
        n += width;
      }
    memcpy (out, base, k);
    const char *hex = "0123456789ABCDEF";
    for (const unsigned char *p = (const unsigned char *)query; *p; ++p)
      {
        unsigned c = *p;
        if (mulib_search_query_plain_ (c))
          out[k++] = (char)c;
        else if (c == ' ')
          out[k++] = '+';
        else
          {
            out[k++] = '%';
            out[k++] = hex[c >> 4];
            out[k++] = hex[c & 15];
          }
      }
    out[k] = 0;
    return 1;
  }

#ifdef __cplusplus
}
#endif
#endif
