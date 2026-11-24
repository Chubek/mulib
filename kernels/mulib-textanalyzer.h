/* MuLIB: Chained tokenization with optional stopword removal. C11 single-header; define
 * MULIB_IMPL_TEXTANALYZER once. */
#ifndef MULIB_TEXTANALYZER_H
#define MULIB_TEXTANALYZER_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-tokenizer.h"
#include "mulib-stopwords.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    mulib_tokenizer_t tokens;
    const mulib_stopwords_t *stopwords;
  } mulib_textanalyzer_t;
  void mulib_textanalyzer_init (mulib_textanalyzer_t *a, const void *text,
                                size_t length, const mulib_stopwords_t *stopwords);
  int mulib_textanalyzer_next (mulib_textanalyzer_t *a, char *out, size_t cap,
                               size_t *length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TEXTANALYZER) && !defined(MULIB_TEXTANALYZER_IMPLEMENTED)
#define MULIB_TEXTANALYZER_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  void
  mulib_textanalyzer_init (mulib_textanalyzer_t *a, const void *s, size_t n,
                           const mulib_stopwords_t *stop)
  {
    if (!a)
      return;
    mulib_tokenizer_init (&a->tokens, s, n);
    a->stopwords = stop;
  }
  int
  mulib_textanalyzer_next (mulib_textanalyzer_t *a, char *out, size_t cap, size_t *len)
  {
    if (!a)
      return -1;
    int rc;
    while ((rc = mulib_tokenizer_next (&a->tokens, out, cap, len)) == 1)
      {
        if (!a->stopwords || !mulib_stopwords_contains (a->stopwords, out))
          return 1;
      }
    return rc;
  }
#ifdef __cplusplus
}
#endif
#endif
