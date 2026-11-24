/** @file mulib-uip-timer.h
 * @brief Monotonic clock helper for userspace IP ARP expiry and retransmission
 * scheduling C11, single-header, define MULIB_IMPL_UIP_TIMER exactly once.
 */
#ifndef MULIB_UIP_TIMER_H
#define MULIB_UIP_TIMER_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_uip_timer_monotonic_ms (uint64_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UIP_TIMER) && !defined(MULIB_UIP_TIMER_IMPLEMENTED)
#define MULIB_UIP_TIMER_IMPLEMENTED
#include <errno.h>
#include <time.h>
int
mulib_uip_timer_monotonic_ms (uint64_t *out)
{
  if (!out)
    return -EINVAL;
  struct timespec t;
#if defined(CLOCK_MONOTONIC)
  if (clock_gettime (CLOCK_MONOTONIC, &t))
    return -errno;
#elif defined(__linux__)
  /* Linux clockid_t is int; strict glibc headers hide its POSIX declaration. */
  extern int clock_gettime (int, struct timespec *);
  if (clock_gettime (1, &t))
    return -errno;
#else
  return -ENOSYS;
#endif
  *out = (uint64_t)t.tv_sec * 1000u + (uint64_t)t.tv_nsec / 1000000u;
  return 0;
}
#endif
