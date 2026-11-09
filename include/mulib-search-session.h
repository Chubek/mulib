/** @file mulib-search-session.h
 * @brief Bounded paginated search offset and page-size tracking.
 * C11 single-header implementation. Define MULIB_IMPL_SEARCH_SESSION in one
 * translation unit.
 */
#ifndef MULIB_SEARCH_SESSION_H
#define MULIB_SEARCH_SESSION_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_search_session
  {
    size_t offset, limit, max_results;
  } mulib_search_session_t;
  /** @brief Initialize positive page size and result bound; 0 invalid,
   * unchanged. */
  int mulib_search_session_init (mulib_search_session_t *s, size_t page_size,
                                 size_t max_results);
  /** @brief Return next page's count and advance offset, or 0
   * exhausted/invalid. The final page may be short; this helper performs no
   * search or I/O. */
  size_t mulib_search_session_next (mulib_search_session_t *s);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SEARCH_SESSION)                                        \
    && !defined(MULIB_SEARCH_SESSION_IMPLEMENTED)
#define MULIB_SEARCH_SESSION_IMPLEMENTED
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
  mulib_search_session_init (mulib_search_session_t *s, size_t size,
                             size_t max)
  {
    if (!s || !size || !max)
      return 0;
    s->offset = 0;
    s->limit = size;
    s->max_results = max;
    return 1;
  }
  size_t
  mulib_search_session_next (mulib_search_session_t *s)
  {
    if (!s || s->offset >= s->max_results)
      return 0;
    size_t n = s->max_results - s->offset;
    if (n > s->limit)
      n = s->limit;
    s->offset += n;
    return n;
  }

#ifdef __cplusplus
}
#endif
#endif
