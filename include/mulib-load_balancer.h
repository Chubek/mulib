/** @file mulib-load_balancer.h
 * @brief Round-robin and least-connections selection over borrowed upstreams.
 * Select does not acquire; caller owns synchronization. SIZE_MAX means none.
 * Define MULIB_IMPL_LOAD_BALANCER and UPSTREAM. */
#ifndef MULIB_LOAD_BALANCER_H
#define MULIB_LOAD_BALANCER_H
#include "mulib-upstream.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  size_t mulib_load_balancer_select (const mulib_upstream_t *items,
                                     size_t count, size_t *cursor,
                                     int least_connections);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LOAD_BALANCER)                                          \
    && !defined(MULIB_LOAD_BALANCER_IMPLEMENTED)
#define MULIB_LOAD_BALANCER_IMPLEMENTED
size_t
mulib_load_balancer_select (const mulib_upstream_t *u, size_t n, size_t *cursor,
                            int least)
{
  size_t best = SIZE_MAX, i;
  if (!u || !n || !cursor)
    return SIZE_MAX;
  i = *cursor % n;
  for (size_t k = 0; k < n; ++k)
    {
      if (mulib_upstream_available (u + i)
          && (best == SIZE_MAX || u[i].active < u[best].active))
        {
          best = i;
          if (!least)
            break;
        }
      i = i == n - 1 ? 0 : i + 1;
    }
  if (best != SIZE_MAX)
    *cursor = best == n - 1 ? 0 : best + 1;
  return best;
}
#endif
