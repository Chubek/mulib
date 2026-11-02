/** @file mulib-metrics.h
 * @brief Saturating single-threaded HTTP counters and latency totals.
 * Define MULIB_IMPL_METRICS. Durations are caller-supplied microseconds. */
#ifndef MULIB_METRICS_H
#define MULIB_METRICS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_metrics
  {
    uint64_t requests, bytes_in, bytes_out, duration_us, status_class[6];
  } mulib_metrics_t;
  int mulib_metrics_record (mulib_metrics_t *metrics, unsigned status,
                            uint64_t input, uint64_t output,
                            uint64_t duration_us);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_METRICS) && !defined(MULIB_METRICS_IMPLEMENTED)
#define MULIB_METRICS_IMPLEMENTED
static uint64_t
mulib_metrics_add_ (uint64_t a, uint64_t b)
{
  return b > UINT64_MAX - a ? UINT64_MAX : a + b;
}
int
mulib_metrics_record (mulib_metrics_t *m, unsigned s, uint64_t in, uint64_t out,
                      uint64_t us)
{
  if (!m || s < 100 || s > 599)
    return 0;
  m->requests = mulib_metrics_add_ (m->requests, 1);
  m->bytes_in = mulib_metrics_add_ (m->bytes_in, in);
  m->bytes_out = mulib_metrics_add_ (m->bytes_out, out);
  m->duration_us = mulib_metrics_add_ (m->duration_us, us);
  m->status_class[s / 100] = mulib_metrics_add_ (m->status_class[s / 100], 1);
  return 1;
}
#endif
