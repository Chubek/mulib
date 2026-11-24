/** @file mulib-scrape-metadata.h
 * @brief Extract bounded HTML meta content attributes by name.
 * C11 single-header implementation. Define MULIB_IMPL_SCRAPE_METADATA in one
 * translation unit.
 */
#ifndef MULIB_SCRAPE_METADATA_H
#define MULIB_SCRAPE_METADATA_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Extract a meta element's quoted content by quoted name. ASCII tag,
   * attribute names and key matching are case-insensitive; either quote style.
   * Comments/scripts/styles ignored; entities remain encoded. 0 absent/error,
   * output unchanged. Input/key/output must not overlap. */
  int mulib_scrape_metadata_get (const char *html, const char *key, char *out,
                                 size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SCRAPE_METADATA)                                       \
    && !defined(MULIB_SCRAPE_METADATA_IMPLEMENTED)
#define MULIB_SCRAPE_METADATA_IMPLEMENTED
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
  mulib_scrape_metadata_get (const char *h, const char *key, char *out,
                             size_t cap)
  {
    if (!h || !key || !out || !*key)
      return 0;
    mulib_webscan_tag_ t;
    while (mulib_webscan_next_ (&h, &t))
      {
        const char *value;
        size_t n;
        if (!t.closing && mulib_webscan_equal_ (t.name, t.length, "meta")
            && mulib_webscan_attr_ (&t, "name", &value, &n)
            && mulib_webscan_equal_ (value, n, key)
            && mulib_webscan_attr_ (&t, "content", &value, &n))
          {
            if (n >= cap)
              return 0;
            memcpy (out, value, n);
            out[n] = 0;
            return 1;
          }
      }
    return 0;
  }

#ifdef __cplusplus
}
#endif
#endif
