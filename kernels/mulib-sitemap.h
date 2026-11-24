/** @file mulib-sitemap.h
 * @brief Extract next loc entry from XML sitemap bytes without fetching.
 * C11 single-header implementation. Define MULIB_IMPL_SITEMAP in one
 * translation unit.
 */
#ifndef MULIB_SITEMAP_H
#define MULIB_SITEMAP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Find literal <loc>...</loc> in exactly len bytes, starting at
   * offset. No XML tree validation, namespace/entity/CDATA handling or comment
   * filtering. 1 copied and offset advanced; 0 exhausted/malformed/too small,
   * both outputs unchanged. Empty loc is rejected. Input/output must not
   * overlap. */
  int mulib_sitemap_next (const char *xml, size_t len, size_t *offset,
                          char *url, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SITEMAP) && !defined(MULIB_SITEMAP_IMPLEMENTED)
#define MULIB_SITEMAP_IMPLEMENTED
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
  mulib_sitemap_next (const char *s, size_t n, size_t *pos, char *out,
                      size_t cap)
  {
    if (!s || !pos || !out || *pos > n)
      return 0;
    for (size_t i = *pos; n - i >= 5; i++)
      if (!memcmp (s + i, "<loc>", 5))
        {
          size_t a = i + 5, j = a;
          while (n - j >= 6 && memcmp (s + j, "</loc>", 6))
            j++;
          if (n - j < 6 || j == a || j - a >= cap || memchr (s + a, 0, j - a)
              || memchr (s + a, '<', j - a))
            return 0;
          memcpy (out, s + a, j - a);
          out[j - a] = 0;
          *pos = j + 6;
          return 1;
        }
    return 0;
  }

#ifdef __cplusplus
}
#endif
#endif
