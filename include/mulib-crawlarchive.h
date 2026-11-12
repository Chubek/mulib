/** @file mulib-crawlarchive.h
 * @brief Owned replayable crawl records with content hashes and bounded
 * payloads. C11 single-header module. Define MULIB_IMPL_CRAWLARCHIVE in
 * exactly one C translation unit.
 *
 */
#ifndef MULIB_CRAWLARCHIVE_H
#define MULIB_CRAWLARCHIVE_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_crawlarchive_record
  {
    char url[1024];
    unsigned status;
    uint64_t hash;
    mulib_shylock_buffer_t body;
  } mulib_crawlarchive_record_t;
  typedef struct mulib_crawlarchive
  {
    mulib_crawlarchive_record_t *items;
    size_t len, cap, max_body;
  } mulib_crawlarchive_t;
  /** @brief See manual/mulib-crawlarchive.md for this operation's contract. */
  int mulib_crawlarchive_add (mulib_crawlarchive_t *a, const char *url,
                              unsigned status, const void *body,
                              size_t length);
  const mulib_crawlarchive_record_t *
  /** @brief See manual/mulib-crawlarchive.md for this operation's contract. */
  mulib_crawlarchive_find (const mulib_crawlarchive_t *a, const char *url);
  /** @brief See manual/mulib-crawlarchive.md for this operation's contract. */
  void mulib_crawlarchive_free (mulib_crawlarchive_t *a);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CRAWLARCHIVE_H */
#ifdef MULIB_IMPL_CRAWLARCHIVE
#ifndef MULIB_IMPL_CRAWLARCHIVE_ONCE
#define MULIB_IMPL_CRAWLARCHIVE_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_crawlarchive_add (mulib_crawlarchive_t *a, const char *url,
                        unsigned status, const void *bytes, size_t n)
{
  if (!a || !url || (!bytes && n) || status < 100 || status > 599
      || (a->max_body && n > a->max_body))
    return 0;
  mulib_crawlarchive_record_t z = { 0 };
  if (!mulib_shylock_copy (z.url, sizeof z.url, url)
      || !mulib_shylock_append (&z.body, bytes, n))
    return 0;
  z.status = status;
  z.hash = mulib_shylock_hash (bytes, n);
  if (a->len == a->cap)
    {
      size_t c = a->cap ? a->cap * 2 : 8;
      if (c < a->cap || c > SIZE_MAX / sizeof z)
        {
          mulib_shylock_buffer_free (&z.body);
          return 0;
        }
      void *p = realloc (a->items, c * sizeof z);
      if (!p)
        {
          mulib_shylock_buffer_free (&z.body);
          return 0;
        }
      a->items = p;
      a->cap = c;
    }
  a->items[a->len++] = z;
  return 1;
}
const mulib_crawlarchive_record_t *
mulib_crawlarchive_find (const mulib_crawlarchive_t *a, const char *url)
{
  if (!a || !url)
    return NULL;
  for (size_t i = a->len; i > 0; i--)
    if (!strcmp (a->items[i - 1].url, url))
      return a->items + i - 1;
  return NULL;
}
void
mulib_crawlarchive_free (mulib_crawlarchive_t *a)
{
  if (a)
    {
      for (size_t i = 0; i < a->len; i++)
        mulib_shylock_buffer_free (&a->items[i].body);
      free (a->items);
      memset (a, 0, sizeof *a);
    }
}
#endif
#endif
