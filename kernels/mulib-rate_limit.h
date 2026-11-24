/** @file mulib-rate_limit.h
 * @brief Caller-clocked token bucket. Single-threaded; time in seconds.
 * Define MULIB_IMPL_RATE_LIMIT. Invalid/backwards times never grant tokens. */
#ifndef MULIB_RATE_LIMIT_H
#define MULIB_RATE_LIMIT_H
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_rate_limit
  {
    double tokens, burst, rate, updated;
  } mulib_rate_limit_t;
  int mulib_rate_limit_init (mulib_rate_limit_t *bucket, double rate,
                             double burst, double now);
  int mulib_rate_limit_take (mulib_rate_limit_t *bucket, double cost,
                             double now);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RATE_LIMIT) && !defined(MULIB_RATE_LIMIT_IMPLEMENTED)
#define MULIB_RATE_LIMIT_IMPLEMENTED
#include <math.h>
int
mulib_rate_limit_init (mulib_rate_limit_t *b, double rate, double burst,
                       double now)
{
  if (!b || !isfinite (rate) || !isfinite (burst) || !isfinite (now) || rate < 0
      || burst <= 0 || now < 0)
    return 0;
  b->tokens = b->burst = burst;
  b->rate = rate;
  b->updated = now;
  return 1;
}
int
mulib_rate_limit_take (mulib_rate_limit_t *b, double cost, double now)
{
  double gain;
  if (!b || !isfinite (cost) || cost <= 0 || !isfinite (now) || now < b->updated
      || !isfinite (b->tokens) || !isfinite (b->burst) || !isfinite (b->rate)
      || !isfinite (b->updated) || b->burst <= 0 || b->rate < 0 || b->tokens < 0
      || b->tokens > b->burst)
    return 0;
  gain = (now - b->updated) * b->rate;
  b->tokens = gain >= b->burst - b->tokens ? b->burst : b->tokens + gain;
  b->updated = now;
  if (cost > b->tokens)
    return 0;
  b->tokens -= cost;
  return 1;
}
#endif
