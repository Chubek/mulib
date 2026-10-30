/** @file mulib-schedutils.h
 * @brief Scheduler inspection, cooperative yield and checked periodic
 * deadlines. Define MULIB_IMPL_SCHEDUTILS once.
 */
#ifndef MULIB_SCHEDUTILS_H
#define MULIB_SCHEDUTILS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Inspect the calling process's native scheduling policy/priority.
   * 0 success, -1 invalid, -2 OS error, -3 unsupported. */
  int mulib_schedutils_current (int *policy, int *priority);
  /** @brief Yield to the operating-system scheduler; same result convention.
   */
  int mulib_schedutils_yield (void);
  /** @brief Compute the first periodic deadline strictly after now from
   * previous. A future previous is retained. period must be nonzero; overflow
   * fails with unchanged output. Times are caller-supplied monotonic ticks. 1
   * success. */
  int mulib_schedutils_next (uint64_t previous, uint64_t period, uint64_t now,
                             uint64_t *next);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SCHEDUTILS) && !defined(MULIB_SCHEDUTILS_IMPLEMENTED)
#define MULIB_SCHEDUTILS_IMPLEMENTED
#if defined(__unix__) || defined(__APPLE__)
#include <sched.h>
#endif
int
mulib_schedutils_current (int *policy, int *priority)
{
  if (!policy || !priority || policy == priority)
    return -1;
#if defined(__unix__) || defined(__APPLE__)
  {
    struct sched_param p;
    int v = sched_getscheduler (0);
    if (v < 0 || sched_getparam (0, &p) < 0)
      return -2;
    *policy = v;
    *priority = p.sched_priority;
    return 0;
  }
#else
  return -3;
#endif
}
int
mulib_schedutils_yield (void)
{
#if defined(__unix__) || defined(__APPLE__)
  return sched_yield () == 0 ? 0 : -2;
#else
  return -3;
#endif
}
int
mulib_schedutils_next (uint64_t prev, uint64_t period, uint64_t now,
                       uint64_t *next)
{
  uint64_t steps;
  if (!next || !period)
    return 0;
  if (prev > now)
    {
      *next = prev;
      return 1;
    }
  steps = (now - prev) / period;
  if (steps == UINT64_MAX || steps + 1 > (UINT64_MAX - prev) / period)
    return 0;
  *next = prev + (steps + 1) * period;
  return 1;
}
#endif
