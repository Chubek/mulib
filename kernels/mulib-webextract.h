/** @file mulib-webextract.h
 * @brief Locate JSON-LD script block within HTML as borrowed byte span.
 * C11 single-header implementation. Define MULIB_IMPL_WEBEXTRACT in one
 * translation unit.
 */
#ifndef MULIB_WEBEXTRACT_H
#define MULIB_WEBEXTRACT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_webextract_span
  {
    const char *data;
    size_t length;
  } mulib_webextract_span_t;
  /** @brief Borrow the first script body with quoted type application/ld+json.
   * ASCII tag/attribute/type case ignored; comments and unrelated script/style
   * contents skipped. No JSON validation. 0 absent/error, output unchanged.
   * Span is not NUL-terminated and is valid only while input remains alive. */
  int mulib_webextract_jsonld (const char *html, mulib_webextract_span_t *out);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WEBEXTRACT) && !defined(MULIB_WEBEXTRACT_IMPLEMENTED)
#define MULIB_WEBEXTRACT_IMPLEMENTED
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
  mulib_webextract_jsonld (const char *s, mulib_webextract_span_t *o)
  {
    if (!s || !o)
      return 0;
    mulib_webscan_tag_ t;
    while (mulib_webscan_next_ (&s, &t))
      {
        const char *type;
        size_t n;
        if (!t.closing && mulib_webscan_equal_ (t.name, t.length, "script")
            && mulib_webscan_attr_ (&t, "type", &type, &n)
            && mulib_webscan_equal_ (type, n, "application/ld+json"))
          {
            const char *end = mulib_webscan_raw_end_ (t.end + 1, "script");
            if (!end)
              return 0;
            o->data = t.end + 1;
            o->length = (size_t)(end - t.end - 1);
            return 1;
          }
      }
    return 0;
  }

#ifdef __cplusplus
}
#endif
#endif
