/* MuLIB: Sorted static stopword set lookup for normalized tokens. C11 single-header;
 * define MULIB_IMPL_STOPWORDS once. */
#ifndef MULIB_STOPWORDS_H
#define MULIB_STOPWORDS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    const char *const *words;
    size_t count;
  } mulib_stopwords_t;
  /* words must be sorted with strcmp; returns 1 if present. */
  int mulib_stopwords_contains (const mulib_stopwords_t *set, const char *word);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_STOPWORDS) && !defined(MULIB_STOPWORDS_IMPLEMENTED)
#define MULIB_STOPWORDS_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_stopwords_contains (const mulib_stopwords_t *s, const char *w)
  {
    if (!s || !w || (!s->words && s->count))
      return 0;
    size_t lo = 0, hi = s->count;
    while (lo < hi)
      {
        size_t m = lo + (hi - lo) / 2;
        int c = strcmp (s->words[m], w);
        if (c < 0)
          lo = m + 1;
        else
          hi = m;
      }
    return lo < s->count && !strcmp (s->words[lo], w);
  }
#ifdef __cplusplus
}
#endif
#endif
