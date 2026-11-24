/** @file mulib-anomaly-detection.h
 * @brief Univariate streaming z-score anomaly detection over Welford statistics.
 * Define MULIB_IMPL_ANOMALY_DETECTION and MULIB_IMPL_STATS; link libm.
 */
#ifndef MULIB_ANOMALY_DETECTION_H
#define MULIB_ANOMALY_DETECTION_H
#include "mulib-stats.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Absolute z score against an existing baseline; does not train on the query.
 * NAN before two samples or for invalid data. Zero variance: 0 at mean, INFINITY otherwise. */
double mulib_anomaly_zscore(const mulib_stats_t *baseline, double value);
/** Return 1 if z>threshold, 0 otherwise, -1 for invalid baseline/value/threshold. */
int mulib_anomaly_detect(const mulib_stats_t *baseline, double value, double threshold);
/** Add a finite training value, rejecting overflow without changing the baseline. */
int mulib_anomaly_train(mulib_stats_t *baseline, double value);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ANOMALY_DETECTION) && !defined(MULIB_ANOMALY_DETECTION_IMPLEMENTED)
#define MULIB_ANOMALY_DETECTION_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
double mulib_anomaly_zscore(const mulib_stats_t *s, double x)
{
  double sd;
  if (!s || s->count < 2 || !isfinite(x) || !isfinite(s->mean) || !isfinite(s->m2) || s->m2 < 0) return NAN;
  sd = mulib_stats_stddev(s);
  if (!sd) return x == s->mean ? 0 : INFINITY;
  return (double)(fabsl((long double)x - s->mean) / sd);
}
int mulib_anomaly_detect(const mulib_stats_t *s, double x, double threshold)
{
  double z = mulib_anomaly_zscore(s, x);
  if (isnan(z) || !isfinite(threshold) || threshold < 0) return -1;
  return z > threshold;
}
int mulib_anomaly_train(mulib_stats_t *s, double x)
{
  mulib_stats_t copy;
  if (!s || !isfinite(x) || s->count == SIZE_MAX || !isfinite(s->mean) || !isfinite(s->m2) || s->m2 < 0) return 0;
  copy = *s; mulib_stats_add(&copy, x);
  if (!isfinite(copy.mean) || !isfinite(copy.m2)) return 0;
  *s = copy; return 1;
}
#ifdef __cplusplus
}
#endif
#endif
