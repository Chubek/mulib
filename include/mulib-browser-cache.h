/** @file mulib-browser-cache.h
 * @brief Aggregate-byte-bounded response cache with expiry and ETag metadata.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_CACHE in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_BROWSER_CACHE_H
#define MULIB_BROWSER_CACHE_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_cache_entry
  {
    char url[1024], etag[256];
    int64_t expires;
    mulib_shylock_buffer_t body;
  } mulib_browser_cache_entry_t;
  typedef struct mulib_browser_cache
  {
    mulib_browser_cache_entry_t *items;
    size_t len, cap, max_entries, max_bytes;
  } mulib_browser_cache_t;
  /** @brief See manual/mulib-browser-cache.md for this operation's contract.
   */
  int mulib_browser_cache_put (mulib_browser_cache_t *c, const char *url,
                               const char *etag, int64_t expiry,
                               const void *data, size_t n);
  const mulib_browser_cache_entry_t *
  /** @brief See manual/mulib-browser-cache.md for this operation's contract.
   */
  mulib_browser_cache_get (const mulib_browser_cache_t *c, const char *url,
                           int64_t now);
  /** @brief Remove expired entries and return their count; borrowed views
   * expire. */
  size_t mulib_browser_cache_prune (mulib_browser_cache_t *cache, int64_t now);
  /** @brief See manual/mulib-browser-cache.md for this operation's contract.
   */
  void mulib_browser_cache_free (mulib_browser_cache_t *c);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_CACHE_H */
#ifdef MULIB_IMPL_BROWSER_CACHE
#ifndef MULIB_IMPL_BROWSER_CACHE_ONCE
#define MULIB_IMPL_BROWSER_CACHE_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const mulib_browser_cache_entry_t *
mulib_browser_cache_get (const mulib_browser_cache_t *c, const char *url,
                         int64_t now)
{
  if (!c || !url)
    return NULL;
  for (size_t i = 0; i < c->len; i++)
    if (!strcmp (c->items[i].url, url) && now < c->items[i].expires)
      return c->items + i;
  return NULL;
}
int
mulib_browser_cache_put (mulib_browser_cache_t *c, const char *url,
                         const char *etag, int64_t expiry, const void *data,
                         size_t n)
{
  if (!c || !url || !*url || !etag || expiry < 0 || strpbrk (etag, "\r\n")
      || (!data && n) || (c->max_bytes && n > c->max_bytes))
    return 0;
  size_t replace = SIZE_MAX, total = n;
  for (size_t i = 0; i < c->len; i++)
    {
      if (!strcmp (url, c->items[i].url))
        {
          replace = i;
          continue;
        }
      if (c->items[i].body.len > SIZE_MAX - total)
        return 0;
      total += c->items[i].body.len;
    }
  if ((c->max_bytes && total > c->max_bytes)
      || (replace == SIZE_MAX && c->max_entries && c->len >= c->max_entries))
    return 0;
  mulib_browser_cache_entry_t e = { 0 };
  e.body.limit = c->max_bytes;
  if (!mulib_shylock_copy (e.url, sizeof e.url, url)
      || !mulib_shylock_copy (e.etag, sizeof e.etag, etag)
      || !mulib_shylock_append (&e.body, data, n))
    {
      mulib_shylock_buffer_free (&e.body);
      return 0;
    }
  e.expires = expiry;
  if (replace != SIZE_MAX)
    {
      mulib_shylock_buffer_free (&c->items[replace].body);
      c->items[replace] = e;
      return 1;
    }
  if (c->len == c->cap)
    {
      size_t k = c->cap ? 2 * c->cap : 16;
      if (k < c->cap || k > SIZE_MAX / sizeof e)
        {
          mulib_shylock_buffer_free (&e.body);
          return 0;
        }
      if (c->max_entries && k > c->max_entries)
        k = c->max_entries;
      void *p = realloc (c->items, k * sizeof e);
      if (!p)
        {
          mulib_shylock_buffer_free (&e.body);
          return 0;
        }
      c->items = p;
      c->cap = k;
    }
  c->items[c->len++] = e;
  return 1;
}
size_t
mulib_browser_cache_prune (mulib_browser_cache_t *c, int64_t now)
{
  if (!c || now < 0)
    return 0;
  size_t kept = 0, old = c->len;
  for (size_t i = 0; i < old; i++)
    {
      if (now >= c->items[i].expires)
        mulib_shylock_buffer_free (&c->items[i].body);
      else
        c->items[kept++] = c->items[i];
    }
  c->len = kept;
  return old - kept;
}
void
mulib_browser_cache_free (mulib_browser_cache_t *c)
{
  if (c)
    {
      for (size_t i = 0; i < c->len; i++)
        mulib_shylock_buffer_free (&c->items[i].body);
      free (c->items);
      memset (c, 0, sizeof *c);
    }
}
#endif
#endif
