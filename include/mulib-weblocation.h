/** @file mulib-weblocation.h
 * @brief Owned current URL state for navigation integration.
 * C11 single-header implementation. Define MULIB_IMPL_WEBLOCATION in one
 * translation unit.
 */
#ifndef MULIB_WEBLOCATION_H
#define MULIB_WEBLOCATION_H
#include "mulib-urlcanon.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_weblocation
  {
    char *href;
  } mulib_weblocation_t;
  /** @brief Copy a validated absolute HTTP(S) URL, preserving spelling. Start
   * zeroed; replacement is alias-safe and failure leaves state unchanged. */
  int mulib_weblocation_set (mulib_weblocation_t *l, const char *href);
  /** @brief Release href/reset state; NULL and repeated calls are safe. */
  void mulib_weblocation_free (mulib_weblocation_t *l);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WEBLOCATION) && !defined(MULIB_WEBLOCATION_IMPLEMENTED)
#define MULIB_WEBLOCATION_IMPLEMENTED
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
  mulib_weblocation_set (mulib_weblocation_t *l, const char *h)
  {
    char normalized[8194];
    if (!l || !mulib_urlcanon_normalize (h, normalized, sizeof normalized))
      return 0;
    size_t n = strlen (h) + 1;
    char *p = (char *)malloc (n);
    if (!p)
      return 0;
    memcpy (p, h, n);
    free (l->href);
    l->href = p;
    return 1;
  }
  void
  mulib_weblocation_free (mulib_weblocation_t *l)
  {
    if (l)
      {
        free (l->href);
        l->href = NULL;
      }
  }

#ifdef __cplusplus
}
#endif
#endif
