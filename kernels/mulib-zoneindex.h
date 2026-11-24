/* MuLIB: Zone-map range pruning over finite per-block numeric min/max.
 * Zones need not be sorted. C11 single-header; define MULIB_IMPL_ZONEINDEX once. */
#ifndef MULIB_ZONEINDEX_H
#define MULIB_ZONEINDEX_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    double low, high;
  } mulib_zoneindex_zone_t;
  /* Returns number of zones intersecting query range; SIZE_MAX on invalid/capacity. */
  size_t mulib_zoneindex_filter (const mulib_zoneindex_zone_t *zones, size_t count,
                                 double low, double high, size_t *blocks,
                                 size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ZONEINDEX) && !defined(MULIB_ZONEINDEX_IMPLEMENTED)
#define MULIB_ZONEINDEX_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif
  size_t
  mulib_zoneindex_filter (const mulib_zoneindex_zone_t *z, size_t n, double lo,
                          double hi, size_t *out, size_t cap)
  {
    if ((!z && n) || (!out && cap) || !isfinite (lo) || !isfinite (hi) || lo > hi)
      return SIZE_MAX;
    size_t count = 0;
    for (size_t i = 0; i < n; i++)
      {
        if (!isfinite (z[i].low) || !isfinite (z[i].high) || z[i].low > z[i].high)
          return SIZE_MAX;
        if (z[i].high >= lo && z[i].low <= hi)
          count++;
      }
    if (count > cap)
      return SIZE_MAX;
    for (size_t i = 0, j = 0; i < n; i++)
      if (z[i].high >= lo && z[i].low <= hi)
        out[j++] = i;
    return count;
  }
#ifdef __cplusplus
}
#endif
#endif
