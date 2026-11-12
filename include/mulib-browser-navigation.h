/** @file mulib-browser-navigation.h
 * @brief Owned bounded navigation history and back/forward traversal.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_NAVIGATION in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_BROWSER_NAVIGATION_H
#define MULIB_BROWSER_NAVIGATION_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_navigation
  {
    char **urls;
    size_t len, cap, current, limit;
  } mulib_browser_navigation_t;
  /** @brief See manual/mulib-browser-navigation.md for this operation's
   * contract. */
  int mulib_browser_navigation_visit (mulib_browser_navigation_t *n,
                                      const char *url);
  /** @brief See manual/mulib-browser-navigation.md for this operation's
   * contract. */
  const char *mulib_browser_navigation_move (mulib_browser_navigation_t *n,
                                             int delta);
  /** @brief See manual/mulib-browser-navigation.md for this operation's
   * contract. */
  void mulib_browser_navigation_free (mulib_browser_navigation_t *n);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_NAVIGATION_H */
#ifdef MULIB_IMPL_BROWSER_NAVIGATION
#ifndef MULIB_IMPL_BROWSER_NAVIGATION_ONCE
#define MULIB_IMPL_BROWSER_NAVIGATION_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_browser_navigation_visit (mulib_browser_navigation_t *n, const char *url)
{
  if (!n || !url || !*url || strlen (url) > 8192
      || (n->len && n->current >= n->len))
    return 0;
  size_t kept = n->len ? n->current + 1 : 0;
  if (n->limit && kept >= n->limit)
    return 0;
  char *s = mulib_shylock_dup (url, strlen (url));
  if (!s)
    return 0;
  if (kept == n->cap)
    {
      size_t c = n->cap ? 2 * n->cap : 16;
      if (c < n->cap || c > SIZE_MAX / sizeof (char *))
        {
          free (s);
          return 0;
        }
      void *p = realloc (n->urls, c * sizeof (char *));
      if (!p)
        {
          free (s);
          return 0;
        }
      n->urls = p;
      n->cap = c;
    }
  for (size_t i = kept; i < n->len; i++)
    free (n->urls[i]);
  n->len = kept;
  n->urls[n->len++] = s;
  n->current = n->len - 1;
  return 1;
}
const char *
mulib_browser_navigation_move (mulib_browser_navigation_t *n, int delta)
{
  if (!n || !n->len || n->current >= n->len)
    return NULL;
  if (delta < 0)
    {
      uint64_t amount = (uint64_t)(-(int64_t)delta);
      if (amount > n->current)
        return NULL;
      n->current -= (size_t)amount;
    }
  else
    {
      if ((size_t)delta >= n->len - n->current)
        return NULL;
      n->current += (size_t)delta;
    }
  return n->urls[n->current];
}
void
mulib_browser_navigation_free (mulib_browser_navigation_t *n)
{
  if (n)
    {
      for (size_t i = 0; i < n->len; i++)
        free (n->urls[i]);
      free (n->urls);
      memset (n, 0, sizeof *n);
    }
}
#endif
#endif
