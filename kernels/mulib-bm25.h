/* MuLIB: Okapi BM25 and BM25F field scoring; finite checked inputs. C11 single-header;
 * define MULIB_IMPL_BM25 once. */
#ifndef MULIB_BM25_H
#define MULIB_BM25_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  double mulib_bm25_idf (double total_documents, double document_frequency);
  double mulib_bm25_score (double tf, double df, double doclen, double avglen,
                           double total, double k1, double b);
  double mulib_bm25f_score (const double *tf, const double *len, const double *avglen,
                            const double *weight, const double *b, size_t fields,
                            double df, double total, double k1);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BM25) && !defined(MULIB_BM25_IMPLEMENTED)
#define MULIB_BM25_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif
  double
  mulib_bm25_idf (double n, double df)
  {
    if (!isfinite (n) || !isfinite (df) || n <= 0 || df < 0 || df > n)
      return NAN;
    return (double)log1pl (((long double)n - df + 0.5L) / ((long double)df + 0.5L));
  }
  double
  mulib_bm25_score (double tf, double df, double len, double avg, double n, double k1,
                    double b)
  {
    if (!isfinite (tf) || !isfinite (len) || !isfinite (avg) || !isfinite (k1)
        || !isfinite (b) || tf < 0 || len < 0 || avg <= 0 || k1 <= 0 || b < 0 || b > 1)
      return NAN;
    double idf = mulib_bm25_idf (n, df);
    if (!isfinite (idf))
      return NAN;
    if (tf == 0)
      return 0;
    long double norm = 1 - (long double)b + (long double)b * len / avg;
    long double denom = (long double)tf + (long double)k1 * norm;
    double score = (double)((long double)idf * ((long double)tf / denom)
                            * ((long double)k1 + 1));
    return isfinite (score) ? score : NAN;
  }
  double
  mulib_bm25f_score (const double *tf, const double *len, const double *avg,
                     const double *w, const double *b, size_t count, double df,
                     double total, double k1)
  {
    if (!tf || !len || !avg || !w || !b || !count || !isfinite (k1) || k1 <= 0)
      return NAN;
    double idf = mulib_bm25_idf (total, df);
    if (!isfinite (idf))
      return NAN;
    long double t = 0;
    for (size_t i = 0; i < count; i++)
      {
        if (!isfinite (tf[i]) || !isfinite (len[i]) || !isfinite (avg[i])
            || !isfinite (w[i]) || !isfinite (b[i]) || tf[i] < 0 || len[i] < 0
            || avg[i] <= 0 || w[i] < 0 || b[i] < 0 || b[i] > 1)
          return NAN;
        long double d = 1 - (long double)b[i] + (long double)b[i] * len[i] / avg[i];
        if (d <= 0)
          return NAN;
        t += (long double)w[i] * tf[i] / d;
      }
    double score = t > 0 ? (double)((long double)idf * ((long double)k1 + 1)
                                    * (t / ((long double)k1 + t)))
                         : 0;
    return isfinite (score) ? score : NAN;
  }
#ifdef __cplusplus
}
#endif
#endif
