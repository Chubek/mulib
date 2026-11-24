/* MuLIB search-rank: patched lexical counts, token-boundary and weighted fields.
 * Preserves mulib_search_rank_count byte-level ASCII semantics. */
#ifndef MULIB_SEARCH_RANK_H
#define MULIB_SEARCH_RANK_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  size_t mulib_search_rank_count (const char *text, const char *query);
  /* Count whole ASCII word matches without overlapping non-word characters. */
  size_t mulib_search_rank_words (const char *text, const char *query);
  /* Finite nonnegative weights; exact match counts may overlap. */
  double mulib_search_rank_fields (const char *title, const char *description,
                                   const char *query, double title_weight,
                                   double description_weight);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SEARCH_RANK) && !defined(MULIB_SEARCH_RANK_IMPLEMENTED)
#define MULIB_SEARCH_RANK_IMPLEMENTED
#include <math.h>
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
  static int
  mulib_search_rank_word_ (unsigned c)
  {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
           || c == '_';
  }
  size_t
  mulib_search_rank_count (const char *s, const char *q)
  {
    if (!s || !q || !*q)
      return 0;
    size_t n = 0, ql = strlen (q);
    for (; *s; ++s)
      {
        size_t i = 0;
        for (; i < ql && s[i]; ++i)
          if (mulib_search_rank_lower_ ((unsigned char)s[i])
              != mulib_search_rank_lower_ ((unsigned char)q[i]))
            break;
        if (i == ql && n < SIZE_MAX)
          ++n;
      }
    return n;
  }
  size_t
  mulib_search_rank_words (const char *s, const char *q)
  {
    if (!s || !q || !*q)
      return 0;
    size_t n = 0, ql = strlen (q);
    if (!ql || !mulib_search_rank_word_ ((unsigned char)q[0])
        || !mulib_search_rank_word_ ((unsigned char)q[ql - 1]))
      return 0;
    for (const char *p = s; *p; ++p)
      {
        if (p != s && mulib_search_rank_word_ ((unsigned char)p[-1]))
          continue;
        size_t i = 0;
        for (; i < ql && p[i]; ++i)
          if (mulib_search_rank_lower_ ((unsigned char)p[i])
              != mulib_search_rank_lower_ ((unsigned char)q[i]))
            break;
        if (i == ql && !mulib_search_rank_word_ ((unsigned char)p[ql]) && n < SIZE_MAX)
          ++n;
      }
    return n;
  }
  double
  mulib_search_rank_fields (const char *title, const char *description,
                            const char *query, double tw, double dw)
  {
    if (!isfinite (tw) || !isfinite (dw) || tw < 0 || dw < 0)
      return NAN;
    double score = (double)mulib_search_rank_words (title, query) * tw
                   + (double)mulib_search_rank_words (description, query) * dw;
    return isfinite (score) ? score : NAN;
  }
#ifdef __cplusplus
}
#endif
#endif
