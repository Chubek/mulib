/** @file mulib-scrape-schema.h
 * @brief Schema-level required string validation for extracted fields.
 * C11 single-header implementation. Define MULIB_IMPL_SCRAPE_SCHEMA in one
 * translation unit.
 */
#ifndef MULIB_SCRAPE_SCHEMA_H
#define MULIB_SCRAPE_SCHEMA_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief 1 if byte length (excluding NUL) fits max_bytes and required
   * values are nonempty. Optional NULL is valid; no UTF-8 or whitespace
   * validation. */
  int mulib_scrape_schema_string (const char *value, int required,
                                  size_t max_bytes);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SCRAPE_SCHEMA)                                         \
    && !defined(MULIB_SCRAPE_SCHEMA_IMPLEMENTED)
#define MULIB_SCRAPE_SCHEMA_IMPLEMENTED
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
  mulib_scrape_schema_string (const char *v, int required, size_t max_bytes)
  {
    if (!v)
      return !required;
    size_t n = strlen (v);
    return n <= max_bytes && (!required || n > 0);
  }

#ifdef __cplusplus
}
#endif
#endif
