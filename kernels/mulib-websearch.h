/** @file mulib-websearch.h
 * @brief Static Google/Bing search provider identification.
 * C11 single-header implementation. Define MULIB_IMPL_WEBSEARCH in one
 * translation unit.
 */
#ifndef MULIB_WEBSEARCH_H
#define MULIB_WEBSEARCH_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum mulib_websearch_provider
  {
    MULIB_WEBSEARCH_GOOGLE = 1,
    MULIB_WEBSEARCH_BING = 2
  } mulib_websearch_provider_t;
  /** @brief Borrow static lowercase provider name, or NULL for an unknown
   * enum. Identification only; no URL parsing, search execution or network
   * access. */
  const char *mulib_websearch_provider_name (mulib_websearch_provider_t p);
  /** @brief Return 1 for GOOGLE/BING, otherwise 0. */
  int mulib_websearch_known (mulib_websearch_provider_t p);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WEBSEARCH) && !defined(MULIB_WEBSEARCH_IMPLEMENTED)
#define MULIB_WEBSEARCH_IMPLEMENTED
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif

  const char *
  mulib_websearch_provider_name (mulib_websearch_provider_t p)
  {
    switch (p)
      {
      case MULIB_WEBSEARCH_GOOGLE:
        return "google";
      case MULIB_WEBSEARCH_BING:
        return "bing";
      default:
        return NULL;
      }
  }
  int
  mulib_websearch_known (mulib_websearch_provider_t p)
  {
    return mulib_websearch_provider_name (p) != NULL;
  }

#ifdef __cplusplus
}
#endif
#endif
