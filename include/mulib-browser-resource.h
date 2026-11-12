/** @file mulib-browser-resource.h
 * @brief Deduplicated resource registry with load state and content type.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_RESOURCE in exactly one
 * C translation unit.
 *
 */
#ifndef MULIB_BROWSER_RESOURCE_H
#define MULIB_BROWSER_RESOURCE_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_resource
  {
    char url[1024], content_type[128];
    unsigned status;
    uint64_t bytes;
    int loaded;
  } mulib_browser_resource_t;
  typedef struct mulib_browser_resources
  {
    mulib_browser_resource_t *items;
    size_t len, cap, limit;
  } mulib_browser_resources_t;
  /** @brief See manual/mulib-browser-resource.md for this operation's
   * contract. */
  int mulib_browser_resource_register (mulib_browser_resources_t *r,
                                       const char *url, size_t *id);
  /** @brief See manual/mulib-browser-resource.md for this operation's
   * contract. */
  int mulib_browser_resource_complete (mulib_browser_resources_t *r, size_t id,
                                       unsigned status, const char *type,
                                       uint64_t bytes);
  /** @brief See manual/mulib-browser-resource.md for this operation's
   * contract. */
  void mulib_browser_resources_free (mulib_browser_resources_t *r);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_RESOURCE_H */
#ifdef MULIB_IMPL_BROWSER_RESOURCE
#ifndef MULIB_IMPL_BROWSER_RESOURCE_ONCE
#define MULIB_IMPL_BROWSER_RESOURCE_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_browser_resource_register (mulib_browser_resources_t *r, const char *url,
                                 size_t *id)
{
  if (!r || !url || !*url || !id)
    return 0;
  for (size_t i = 0; i < r->len; i++)
    if (!strcmp (r->items[i].url, url))
      {
        *id = i;
        return 1;
      }
  if (r->limit && r->len >= r->limit)
    return 0;
  mulib_browser_resource_t v = { 0 };
  if (!mulib_shylock_copy (v.url, sizeof v.url, url))
    return 0;
  if (r->len == r->cap)
    {
      size_t c = r->cap ? 2 * r->cap : 16;
      if (c < r->cap || c > SIZE_MAX / sizeof v)
        return 0;
      if (r->limit && c > r->limit)
        c = r->limit;
      void *p = realloc (r->items, c * sizeof v);
      if (!p)
        return 0;
      r->items = p;
      r->cap = c;
    }
  *id = r->len;
  r->items[r->len++] = v;
  return 1;
}
int
mulib_browser_resource_complete (mulib_browser_resources_t *r, size_t id,
                                 unsigned st, const char *ct, uint64_t size)
{
  if (!r || id >= r->len || !ct || strpbrk (ct, "\r\n") || st < 100 || st > 599
      || r->items[id].loaded)
    return 0;
  mulib_browser_resource_t *v = r->items + id;
  if (!mulib_shylock_copy (v->content_type, sizeof v->content_type, ct))
    return 0;
  v->status = st;
  v->bytes = size;
  v->loaded = 1;
  return 1;
}
void
mulib_browser_resources_free (mulib_browser_resources_t *r)
{
  if (r)
    {
      free (r->items);
      memset (r, 0, sizeof *r);
    }
}
#endif
#endif
