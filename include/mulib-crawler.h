/** @file mulib-crawler.h
 * @brief Bounded crawl frontier with retry scheduling and URL deduplication.
 * C11 single-header module. Define MULIB_IMPL_CRAWLER in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_CRAWLER_H
#define MULIB_CRAWLER_H
#include "mulib-browser-origin.h"
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_crawler_job
  {
    char url[1024];
    int64_t due_ms;
    unsigned attempts;
    uint8_t state;
  } mulib_crawler_job_t;
  typedef struct mulib_crawler
  {
    mulib_crawler_job_t *items;
    size_t len, cap, limit;
  } mulib_crawler_t;
  /** @brief See manual/mulib-crawler.md for this operation's contract. */
  int mulib_crawler_enqueue (mulib_crawler_t *c, const char *url, int64_t due);
  /** @brief See manual/mulib-crawler.md for this operation's contract. */
  int mulib_crawler_next (mulib_crawler_t *c, int64_t now, size_t *id);
  /** @brief See manual/mulib-crawler.md for this operation's contract. */
  int mulib_crawler_finish (mulib_crawler_t *c, size_t id, int success,
                            int64_t retry_at, unsigned max_attempts);
  /** @brief See manual/mulib-crawler.md for this operation's contract. */
  void mulib_crawler_free (mulib_crawler_t *c);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CRAWLER_H */
#ifdef MULIB_IMPL_CRAWLER
#ifndef MULIB_IMPL_CRAWLER_ONCE
#define MULIB_IMPL_CRAWLER_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_crawler_enqueue (mulib_crawler_t *c, const char *url, int64_t due)
{
  mulib_browser_origin_t origin;
  if (!c || !url || due < 0 || !mulib_browser_origin_parse (url, &origin))
    return 0;
  for (size_t i = 0; i < c->len; i++)
    if (!strcmp (c->items[i].url, url))
      return 1;
  if (c->limit && c->len >= c->limit)
    return 0;
  mulib_crawler_job_t j = { 0 };
  if (!mulib_shylock_copy (j.url, sizeof j.url, url))
    return 0;
  j.due_ms = due;
  if (c->len == c->cap)
    {
      size_t n = c->cap ? 2 * c->cap : 16;
      if (n < c->cap || n > SIZE_MAX / sizeof j)
        return 0;
      void *p = realloc (c->items, n * sizeof j);
      if (!p)
        return 0;
      c->items = p;
      c->cap = n;
    }
  c->items[c->len++] = j;
  return 1;
}
int
mulib_crawler_next (mulib_crawler_t *c, int64_t now, size_t *id)
{
  if (!c || !id || now < 0)
    return 0;
  size_t best = SIZE_MAX;
  for (size_t i = 0; i < c->len; i++)
    if (!c->items[i].state && c->items[i].attempts < UINT_MAX
        && c->items[i].due_ms <= now
        && (best == SIZE_MAX || c->items[i].due_ms < c->items[best].due_ms))
      best = i;
  if (best == SIZE_MAX)
    return 0;
  c->items[best].state = 1;
  c->items[best].attempts++;
  *id = best;
  return 1;
}
int
mulib_crawler_finish (mulib_crawler_t *c, size_t id, int success,
                      int64_t retry_at, unsigned max)
{
  if (!c || id >= c->len || c->items[id].state != 1 || !max)
    return 0;
  mulib_crawler_job_t *j = c->items + id;
  int done = success || j->attempts >= max;
  if (!done && retry_at < j->due_ms)
    return 0;
  j->state = done ? 2 : 0;
  if (!done)
    j->due_ms = retry_at;
  return 1;
}
void
mulib_crawler_free (mulib_crawler_t *c)
{
  if (c)
    {
      free (c->items);
      memset (c, 0, sizeof *c);
    }
}
#endif
#endif
