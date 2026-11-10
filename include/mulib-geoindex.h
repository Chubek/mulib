/* MuLIB: Exact spherical great-circle distance and radius search over owned geographic
 * points. C11 single-header; define MULIB_IMPL_GEOINDEX once. */
#ifndef MULIB_GEOINDEX_H
#define MULIB_GEOINDEX_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t id;
    double latitude, longitude;
  } mulib_geoindex_point_t;
  typedef struct
  {
    mulib_geoindex_point_t *points;
    size_t count, capacity;
  } mulib_geoindex_t;
  int mulib_geoindex_add (mulib_geoindex_t *g, uint64_t id, double latitude,
                          double longitude);
  double mulib_geoindex_distance_km (double lat1, double lon1, double lat2,
                                     double lon2);
  size_t mulib_geoindex_radius (const mulib_geoindex_t *g, double lat, double lon,
                                double radius_km, uint64_t *ids, size_t capacity);
  void mulib_geoindex_free (mulib_geoindex_t *g);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_GEOINDEX) && !defined(MULIB_GEOINDEX_IMPLEMENTED)
#define MULIB_GEOINDEX_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_geoindex_coord_ (double lat, double lon)
  {
    return isfinite (lat) && isfinite (lon) && lat >= -90 && lat <= 90 && lon >= -180
           && lon <= 180;
  }
  double
  mulib_geoindex_distance_km (double a, double b, double c, double d)
  {
    if (!mulib_geoindex_coord_ (a, b) || !mulib_geoindex_coord_ (c, d))
      return NAN;
    double p = 0.017453292519943295;
    double h
        = sin ((c - a) * p / 2) * sin ((c - a) * p / 2)
          + cos (a * p) * cos (c * p) * sin ((d - b) * p / 2) * sin ((d - b) * p / 2);
    return 12742.0 * asin (sqrt (fmin (1.0, h)));
  }
  int
  mulib_geoindex_add (mulib_geoindex_t *g, uint64_t id, double lat, double lon)
  {
    if (!g || !mulib_geoindex_coord_ (lat, lon))
      return 0;
    for (size_t i = 0; i < g->count; i++)
      if (g->points[i].id == id)
        {
          g->points[i].latitude = lat;
          g->points[i].longitude = lon;
          return 1;
        }
    if (g->count == g->capacity)
      {
        size_t nc = g->capacity ? g->capacity * 2 : 16;
        if (nc < g->capacity || nc > SIZE_MAX / sizeof (*g->points))
          return 0;
        void *p = realloc (g->points, nc * sizeof (*g->points));
        if (!p)
          return 0;
        g->points = (mulib_geoindex_point_t *)p;
        g->capacity = nc;
      }
    g->points[g->count++] = (mulib_geoindex_point_t){ id, lat, lon };
    return 1;
  }
  size_t
  mulib_geoindex_radius (const mulib_geoindex_t *g, double lat, double lon,
                         double radius, uint64_t *out, size_t cap)
  {
    if (!g || !mulib_geoindex_coord_ (lat, lon) || !isfinite (radius) || radius < 0
        || (!out && cap))
      return SIZE_MAX;
    size_t count = 0;
    for (size_t i = 0; i < g->count; i++)
      if (mulib_geoindex_distance_km (lat, lon, g->points[i].latitude,
                                      g->points[i].longitude)
          <= radius)
        count++;
    if (count > cap)
      return SIZE_MAX;
    for (size_t i = 0, j = 0; i < g->count; i++)
      if (mulib_geoindex_distance_km (lat, lon, g->points[i].latitude,
                                      g->points[i].longitude)
          <= radius)
        out[j++] = g->points[i].id;
    return count;
  }
  void
  mulib_geoindex_free (mulib_geoindex_t *g)
  {
    if (!g)
      return;
    free (g->points);
    memset (g, 0, sizeof (*g));
  }
#ifdef __cplusplus
}
#endif
#endif
