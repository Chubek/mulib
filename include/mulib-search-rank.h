/** @file mulib-search-rank.h
 * @brief Deterministic lexical score for ranking title/description matches.
 * C11 single-header implementation. Define MULIB_IMPL_SEARCH_RANK in one
 * translation unit.
 */
#ifndef MULIB_SEARCH_RANK_H
#define MULIB_SEARCH_RANK_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Count overlapping ASCII case-insensitive substring occurrences.
   * No tokenization, Unicode case folding or relevance model. NULL/empty query
   * returns zero; the count saturates at SIZE_MAX. */
  size_t mulib_search_rank_count (const char *text, const char *query);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SEARCH_RANK) && !defined(MULIB_SEARCH_RANK_IMPLEMENTED)
#define MULIB_SEARCH_RANK_IMPLEMENTED
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif

  static unsigned
  mulib_search_rank_lower_ (unsigned c)
  {
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
  }
  size_t
  mulib_search_rank_count (const char *s, const char *q)
  {
    if (!s || !q || !*q)
      return 0;
    size_t n = 0, ql = strlen (q);
    for (; *s; s++)
      {
        size_t i = 0;
        for (; i < ql && s[i]; i++)
          if (mulib_search_rank_lower_ ((unsigned char)s[i])
              != mulib_search_rank_lower_ ((unsigned char)q[i]))
            break;
        if (i == ql && n < SIZE_MAX)
          n++;
      }
    return n;
  }

#ifdef __cplusplus
}
#endif
#endif
