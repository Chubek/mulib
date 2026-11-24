/* MuLIB: TF-IDF classic and sublinear formulas with log-smoothed IDF. C11
 * single-header; define MULIB_IMPL_TFIDF once. */
#ifndef MULIB_TFIDF_H
#define MULIB_TFIDF_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  double mulib_tfidf_idf (double documents, double document_frequency);
  double mulib_tfidf_score (double term_frequency, double documents,
                            double document_frequency, int sublinear);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TFIDF) && !defined(MULIB_TFIDF_IMPLEMENTED)
#define MULIB_TFIDF_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif
  double
  mulib_tfidf_idf (double n, double df)
  {
    if (!isfinite (n) || !isfinite (df) || n < 0 || df < 0 || df > n)
      return NAN;
    return log ((n + 1) / (df + 1)) + 1;
  }
  double
  mulib_tfidf_score (double tf, double n, double df, int sublinear)
  {
    if (!isfinite (tf) || tf < 0)
      return NAN;
    double idf = mulib_tfidf_idf (n, df);
    if (!isfinite (idf))
      return NAN;
    double score = tf == 0 ? 0 : (sublinear ? 1 + log (tf) : tf) * idf;
    return isfinite (score) ? score : NAN;
  }
#ifdef __cplusplus
}
#endif
#endif
