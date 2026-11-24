/** @file mulib-scrape-readable.h
 * @brief Remove markup and collapse HTML whitespace into readable text (basic
 * subset). C11 single-header implementation. Define MULIB_IMPL_SCRAPE_READABLE
 * in one translation unit.
 */
#ifndef MULIB_SCRAPE_READABLE_H
#define MULIB_SCRAPE_READABLE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Drop complete tags, comments and script/style bodies; collapse
   * ASCII HTML whitespace and insert spaces at tag boundaries. Entities not
   * decoded. NUL-terminated truncation to cap-1; returns bytes written, 0
   * empty/invalid. Exact in-place use supported; other overlap unsupported. */
  size_t mulib_scrape_readable (const char *html, char *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SCRAPE_READABLE)                                       \
    && !defined(MULIB_SCRAPE_READABLE_IMPLEMENTED)
#define MULIB_SCRAPE_READABLE_IMPLEMENTED
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
  mulib_scrape_readable (const char *s, char *o, size_t cap)
  {
    if (!s || !o || !cap)
      return 0;
    size_t k = 0;
    int space = 0;
    while (*s)
      {
        if (*s == '<')
          {
            if (!strncmp (s, "<!--", 4))
              {
                const char *end = strstr (s + 4, "-->");
                if (!end)
                  break;
                s = end + 3;
                space = k != 0;
                continue;
              }
            if (s[1] == '!' || s[1] == '?')
              {
                const char *end = mulib_webscan_declaration_end_ (s + 2);
                if (!end)
                  break;
                s = end + 1;
                space = k != 0;
                continue;
              }
            mulib_webscan_tag_ t;
            if (mulib_webscan_tag_parse_ (s, &t))
              {
                s = t.end + 1;
                if (!t.closing
                    && (mulib_webscan_equal_ (t.name, t.length, "script")
                        || mulib_webscan_equal_ (t.name, t.length, "style")))
                  {
                    const char *end = mulib_webscan_raw_end_ (
                        s, mulib_webscan_equal_ (t.name, t.length, "script")
                               ? "script"
                               : "style");
                    if (!end)
                      break;
                    mulib_webscan_tag_ close;
                    if (!mulib_webscan_tag_parse_ (end, &close))
                      break;
                    s = close.end + 1;
                  }
                space = k != 0;
                continue;
              }
          }
        unsigned char c = (unsigned char)*s++;
        if (mulib_webscan_space_ (c))
          {
            space = k != 0;
            continue;
          }
        if (space)
          {
            /* Do not emit a trailing separator without room for the following
             * byte. */
            if (cap - k <= 2)
              break;
            o[k++] = ' ';
            space = 0;
          }
        if (k == cap - 1)
          break;
        o[k++] = (char)c;
      }
    o[k] = 0;
    return k;
  }

#ifdef __cplusplus
}
#endif
#endif
