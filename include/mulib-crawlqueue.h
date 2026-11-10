/* MuLIB crawlqueue: patched bounded owned FIFO, deduplication and cursor control.
 * Existing push/next/free functions and structure layout preserved. */
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
  int mulib_crawlqueue_push (mulib_crawlqueue_t *q, const char *url);
  const char *mulib_crawlqueue_next (mulib_crawlqueue_t *q);
  void mulib_crawlqueue_free (mulib_crawlqueue_t *q);
  /* New: inspect progress without mutation and resume from a saved cursor. */
  size_t mulib_crawlqueue_pending (const mulib_crawlqueue_t *q);
  size_t mulib_crawlqueue_position (const mulib_crawlqueue_t *q);
  int mulib_crawlqueue_seek (mulib_crawlqueue_t *q, size_t cursor);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CRAWLQUEUE) && !defined(MULIB_CRAWLQUEUE_IMPLEMENTED)
#define MULIB_CRAWLQUEUE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_crawlqueue_push (mulib_crawlqueue_t *q, const char *url)
  {
    if (!q || !url || !*url)
      return 0;
    size_t n = strlen (url);
    if (n > 8192)
      return 0;
    for (size_t i = 0; i < q->len; i++)
      if (!strcmp (q->urls[i], url))
        return 2;
    if (q->len >= 100000)
      return 0;
    /* Allocate URL before growth so failures leave all state unchanged. */
    char *copy = (char *)malloc (n + 1);
    if (!copy)
      return 0;
    memcpy (copy, url, n + 1);
    if (q->len == q->cap)
      {
        size_t newcap = q->cap ? q->cap * 2 : 16;
        if (newcap < q->cap || newcap > SIZE_MAX / sizeof (char *))
          {
            free (copy);
            return 0;
          }
        char **p = (char **)realloc (q->urls, newcap * sizeof (char *));
        if (!p)
          {
            free (copy);
            return 0;
          }
        q->urls = p;
        q->cap = newcap;
      }
    q->urls[q->len++] = copy;
    return 1;
  }
  const char *
  mulib_crawlqueue_next (mulib_crawlqueue_t *q)
  {
    return q && q->read < q->len ? q->urls[q->read++] : NULL;
  }
  size_t
  mulib_crawlqueue_pending (const mulib_crawlqueue_t *q)
  {
    return q && q->read <= q->len ? q->len - q->read : 0;
  }
  size_t
  mulib_crawlqueue_position (const mulib_crawlqueue_t *q)
  {
    return q ? q->read : 0;
  }
  int
  mulib_crawlqueue_seek (mulib_crawlqueue_t *q, size_t cursor)
  {
    if (!q || cursor > q->len)
      return 0;
    q->read = cursor;
    return 1;
  }
  void
  mulib_crawlqueue_free (mulib_crawlqueue_t *q)
  {
    if (!q)
      return;
    for (size_t i = 0; i < q->len; i++)
      free (q->urls[i]);
    free (q->urls);
    memset (q, 0, sizeof (*q));
  }
#ifdef __cplusplus
}
#endif
#endif
