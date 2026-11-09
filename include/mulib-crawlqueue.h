/** @file mulib-crawlqueue.h
 * @brief Owned bounded FIFO frontier with URL deduplication.
 * C11 single-header implementation. Define MULIB_IMPL_CRAWLQUEUE in one
 * translation unit.
 */
#ifndef MULIB_CRAWLQUEUE_H
#define MULIB_CRAWLQUEUE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_crawlqueue
  {
    char **urls;
    size_t len, read, cap;
  } mulib_crawlqueue_t;
  /** @brief Start zeroed. Copy URLs (1..8192 bytes), retaining all visited
   * URLs for lifetime deduplication; 100000 total maximum. 1 inserted, 2
   * duplicate, 0 failure. Comparison is byte-exact; no normalization or URL
   * validation. */
  int mulib_crawlqueue_push (mulib_crawlqueue_t *q, const char *url);
  /** @brief Borrow the next FIFO URL until free, or NULL when exhausted. */
  const char *mulib_crawlqueue_next (mulib_crawlqueue_t *q);
  /** @brief Release all URLs/reset state; NULL and repeated calls are safe. */
  void mulib_crawlqueue_free (mulib_crawlqueue_t *q);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CRAWLQUEUE) && !defined(MULIB_CRAWLQUEUE_IMPLEMENTED)
#define MULIB_CRAWLQUEUE_IMPLEMENTED
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
  mulib_crawlqueue_push (mulib_crawlqueue_t *q, const char *s)
  {
    if (!q || !s || !*s || strlen (s) > 8192)
      return 0;
    for (size_t i = 0; i < q->len; i++)
      if (!strcmp (q->urls[i], s))
        return 2;
    if (q->len >= 100000)
      return 0;
    if (q->len == q->cap)
      {
        size_t nc = q->cap ? q->cap * 2 : 16;
        char **p = (char **)realloc (q->urls, nc * sizeof (char *));
        if (!p)
          return 0;
        q->urls = p;
        q->cap = nc;
      }
    size_t n = strlen (s) + 1;
    char *p = (char *)malloc (n);
    if (!p)
      return 0;
    memcpy (p, s, n);
    q->urls[q->len++] = p;
    return 1;
  }
  const char *
  mulib_crawlqueue_next (mulib_crawlqueue_t *q)
  {
    return q && q->read < q->len ? q->urls[q->read++] : NULL;
  }
  void
  mulib_crawlqueue_free (mulib_crawlqueue_t *q)
  {
    if (!q)
      return;
    for (size_t i = 0; i < q->len; i++)
      free (q->urls[i]);
    free (q->urls);
    memset (q, 0, sizeof *q);
  }

#ifdef __cplusplus
}
#endif
#endif
