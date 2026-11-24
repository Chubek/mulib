/** @file mulib-scrape.h
 * @brief Simple query-free HTML text extraction orchestrator.
 * C11 single-header implementation. Define MULIB_IMPL_SCRAPE in one
 * translation unit.
 */
#ifndef MULIB_SCRAPE_H
#define MULIB_SCRAPE_H
#include "mulib-scrape-readable.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Delegate to mulib_scrape_readable; requires SCRAPE_READABLE too.
   * Drops scripts/styles/comments and collapses ASCII HTML whitespace.
   * Returns bytes written, truncating with NUL termination when cap is
   * nonzero. */
  size_t mulib_scrape_text (const char *html, char *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SCRAPE) && !defined(MULIB_SCRAPE_IMPLEMENTED)
#define MULIB_SCRAPE_IMPLEMENTED
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
  mulib_scrape_text (const char *s, char *o, size_t cap)
  {
    return mulib_scrape_readable (s, o, cap);
  }

#ifdef __cplusplus
}
#endif
#endif
