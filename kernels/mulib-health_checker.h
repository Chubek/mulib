/** @file mulib-health_checker.h
 * @brief Consecutive-result health hysteresis, not an active network probe.
 * Define MULIB_IMPL_HEALTH_CHECKER. Initialize counters to zero. */
#ifndef MULIB_HEALTH_CHECKER_H
#define MULIB_HEALTH_CHECKER_H
#include "mulib-upstream.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_health_checker
  {
    unsigned successes, failures, rise, fall;
  } mulib_health_checker_t;
  /** Nonzero rise/fall required. Returns 1 on valid input. */
  int mulib_health_checker_record (mulib_health_checker_t *check,
                                   mulib_upstream_t *upstream, int success);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HEALTH_CHECKER)                                         \
    && !defined(MULIB_HEALTH_CHECKER_IMPLEMENTED)
#define MULIB_HEALTH_CHECKER_IMPLEMENTED
int
mulib_health_checker_record (mulib_health_checker_t *c, mulib_upstream_t *u,
                             int ok)
{
  if (!c || !u || !c->rise || !c->fall)
    return 0;
  if (ok)
    {
      c->failures = 0;
      if (c->successes < c->rise)
        ++c->successes;
      if (c->successes >= c->rise)
        u->healthy = 1;
    }
  else
    {
      c->successes = 0;
      if (c->failures < c->fall)
        ++c->failures;
      if (c->failures >= c->fall)
        u->healthy = 0;
    }
  return 1;
}
#endif
