/** @file mulib-browser-fetch.h
 * @brief Browser fetch request adapter with origin and security policy checks.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_FETCH in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_BROWSER_FETCH_H
#define MULIB_BROWSER_FETCH_H
#include "mulib-browser-origin.h"
#include "mulib-fetch.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_fetch
  {
    mulib_fetch_client_t client;
    mulib_browser_origin_t initiator;
    int enforce_same_origin;
  } mulib_browser_fetch_t;
  /** @brief See manual/mulib-browser-fetch.md for this operation's contract.
   */
  int mulib_browser_fetch_get (const mulib_browser_fetch_t *f, const char *url,
                               mulib_shylock_buffer_t *body, unsigned *status,
                               char *content_type, size_t cap);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_FETCH_H */
#ifdef MULIB_IMPL_BROWSER_FETCH
#ifndef MULIB_IMPL_BROWSER_FETCH_ONCE
#define MULIB_IMPL_BROWSER_FETCH_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_browser_fetch_get (const mulib_browser_fetch_t *f, const char *url,
                         mulib_shylock_buffer_t *body, unsigned *status,
                         char *type, size_t cap)
{
  if (!f)
    return 0;
  mulib_browser_origin_t o;
  if (!mulib_browser_origin_parse (url, &o))
    return 0;
  if (f->enforce_same_origin
      && !mulib_browser_origin_equal (&f->initiator, &o))
    return 0;
  return mulib_fetch_get (&f->client, url, body, status, type, cap);
}
#endif
#endif
