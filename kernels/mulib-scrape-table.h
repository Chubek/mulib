/** @file mulib-scrape-table.h
 * @brief Simple HTML table cell counting for static document inspection.
 * C11 single-header implementation. Define MULIB_IMPL_SCRAPE_TABLE in one
 * translation unit.
 */
#ifndef MULIB_SCRAPE_TABLE_H
#define MULIB_SCRAPE_TABLE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Count complete td/th opening tags, ignoring ASCII case, comments,
   * script/style contents and quoted markup. Does not validate table nesting.
   * NULL/empty input returns 0; no allocation or HTML5 error recovery. */
  size_t mulib_scrape_table_cells (const char *html);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SCRAPE_TABLE)                                          \
    && !defined(MULIB_SCRAPE_TABLE_IMPLEMENTED)
#define MULIB_SCRAPE_TABLE_IMPLEMENTED
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

  size_t
  mulib_scrape_table_cells (const char *s)
  {
    if (!s)
      return 0;
    size_t n = 0;
    mulib_webscan_tag_ t;
    while (mulib_webscan_next_ (&s, &t))
      if (!t.closing
          && (mulib_webscan_equal_ (t.name, t.length, "td")
              || mulib_webscan_equal_ (t.name, t.length, "th")))
        ++n;
    return n;
  }

#ifdef __cplusplus
}
#endif
#endif
