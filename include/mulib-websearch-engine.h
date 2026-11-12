/** @file mulib-websearch-engine.h
 * @brief Deduplicated bounded ranked web search result collection.
 * C11 single-header module. Define MULIB_IMPL_WEBSEARCH_ENGINE in exactly one
 * C translation unit.
 *
 */
#ifndef MULIB_WEBSEARCH_ENGINE_H
#define MULIB_WEBSEARCH_ENGINE_H
#include "mulib-browser-origin.h"
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_websearch_hit
  {
    char url[1024], title[256];
    double score;
    uint32_t providers;
  } mulib_websearch_hit_t;
  typedef struct mulib_websearch_engine
  {
    mulib_websearch_hit_t *hits;
    size_t len, cap, limit;
  } mulib_websearch_engine_t;
  /** @brief See manual/mulib-websearch-engine.md for this operation's
   * contract. */
  int mulib_websearch_engine_add (mulib_websearch_engine_t *e, const char *url,
                                  const char *title, double score,
                                  uint32_t provider_flag);
  /** @brief See manual/mulib-websearch-engine.md for this operation's
   * contract. */
  void mulib_websearch_engine_sort (mulib_websearch_engine_t *e);
  /** @brief See manual/mulib-websearch-engine.md for this operation's
   * contract. */
  void mulib_websearch_engine_free (mulib_websearch_engine_t *e);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_WEBSEARCH_ENGINE_H */
#ifdef MULIB_IMPL_WEBSEARCH_ENGINE
#ifndef MULIB_IMPL_WEBSEARCH_ENGINE_ONCE
#define MULIB_IMPL_WEBSEARCH_ENGINE_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_websearch_engine_add (mulib_websearch_engine_t *e, const char *url,
                            const char *title, double score, uint32_t provider)
{
  mulib_browser_origin_t origin;
  if (!e || !url || !title || !isfinite (score) || score < 0
      || !mulib_browser_origin_parse (url, &origin))
    return 0;
  for (size_t i = 0; i < e->len; i++)
    if (!strcmp (url, e->hits[i].url))
      {
        if (score > e->hits[i].score)
          e->hits[i].score = score;
        e->hits[i].providers |= provider;
        return 1;
      }
  if (e->limit && e->len >= e->limit)
    return 0;
  mulib_websearch_hit_t v = { 0 };
  if (!mulib_shylock_copy (v.url, sizeof v.url, url)
      || !mulib_shylock_copy (v.title, sizeof v.title, title))
    return 0;
  v.score = score;
  v.providers = provider;
  if (e->len == e->cap)
    {
      size_t n = e->cap ? e->cap * 2 : 16;
      if (n < e->cap || n > SIZE_MAX / sizeof v)
        return 0;
      void *t = realloc (e->hits, n * sizeof v);
      if (!t)
        return 0;
      e->hits = t;
      e->cap = n;
    }
  e->hits[e->len++] = v;
  return 1;
}
static int
mulib_websearch_engine_cmp_ (const void *a, const void *b)
{
  const mulib_websearch_hit_t *x = a, *y = b;
  int order = (x->score < y->score) - (x->score > y->score);
  return order ? order : strcmp (x->url, y->url);
}
void
mulib_websearch_engine_sort (mulib_websearch_engine_t *e)
{
  if (e && e->len > 1)
    qsort (e->hits, e->len, sizeof *e->hits, mulib_websearch_engine_cmp_);
}
void
mulib_websearch_engine_free (mulib_websearch_engine_t *e)
{
  if (e)
    {
      free (e->hits);
      memset (e, 0, sizeof *e);
    }
}
#endif
#endif
