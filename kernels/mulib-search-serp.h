/** @file mulib-search-serp.h
 * @brief Extract href attributes from simple quoted HTML anchor markup.
 * C11 single-header implementation. Define MULIB_IMPL_SEARCH_SERP in one
 * translation unit.
 */
#ifndef MULIB_SEARCH_SERP_H
#define MULIB_SEARCH_SERP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Copy the zero-based Nth anchor's quoted href (either quote style).
   * ASCII tag/attribute case ignored; comments/scripts/styles skipped. No
   * entity decoding or URL resolution. 0 absent/error, output unchanged; no
   * overlap. */
  int mulib_search_serp_href (const char *html, size_t index, char *out,
                              size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SEARCH_SERP) && !defined(MULIB_SEARCH_SERP_IMPLEMENTED)
#define MULIB_SEARCH_SERP_IMPLEMENTED
#include "detail/mulib-webscan.h"
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
  mulib_search_serp_href (const char *s, size_t ix, char *o, size_t cap)
  {
    if (!s || !o)
      return 0;
    mulib_webscan_tag_ t;
    while (mulib_webscan_next_ (&s, &t))
      {
        const char *value;
        size_t n;
        if (!t.closing && mulib_webscan_equal_ (t.name, t.length, "a")
            && mulib_webscan_attr_ (&t, "href", &value, &n))
          {
            if (ix)
              {
                --ix;
                continue;
              }
            if (n >= cap)
              return 0;
            memcpy (o, value, n);
            o[n] = 0;
            return 1;
          }
      }
    return 0;
  }

#ifdef __cplusplus
}
#endif
#endif
