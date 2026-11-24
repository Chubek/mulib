/**
 * @file mulib-cgeom.h
 * @brief Computational geometry, convex hulls and SIMD point distances.
 * Define MULIB_IMPL_CGEOM, MULIB_IMPL_LINALG and MULIB_IMPL_SIMDMATH.
 * Predicates use double intermediates, not adaptive exact arithmetic.
 */
#ifndef MULIB_CGEOM_H
#define MULIB_CGEOM_H
#include "mulib-linalg.h"
#include "mulib-simdmath.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_cgeom_ray
  {
    mulib_linalg_vec3_t origin, direction;
  } mulib_cgeom_ray_t;
  typedef struct mulib_cgeom_aabb
  {
    mulib_linalg_vec3_t min, max;
  } mulib_cgeom_aabb_t;
  typedef struct mulib_cgeom_sphere
  {
    mulib_linalg_vec3_t center;
    float radius;
  } mulib_cgeom_sphere_t;
  typedef struct mulib_cgeom_triangle
  {
    mulib_linalg_vec3_t a, b, c;
  } mulib_cgeom_triangle_t;
  /** @brief Finite component/shape checks; boxes may be flat, spheres may have
   * radius zero. */
  int mulib_cgeom_vec3_valid (mulib_linalg_vec3_t v);
  int mulib_cgeom_ray_valid (mulib_cgeom_ray_t ray);
  int mulib_cgeom_aabb_valid (mulib_cgeom_aabb_t box);
  int mulib_cgeom_sphere_valid (mulib_cgeom_sphere_t sphere);
  /** @brief Signed twice-area; positive means counterclockwise. */
  double mulib_cgeom_orient2 (mulib_linalg_vec2_t a, mulib_linalg_vec2_t b,
                              mulib_linalg_vec2_t c);
  /** @brief Signed polygon area; NAN for invalid/nonfinite input, zero for
   * fewer than 3 points. */
  double mulib_cgeom_polygon_area (const mulib_linalg_vec2_t *points,
                                   size_t count);
  /** @brief Even-odd point inclusion, including boundary; -1 invalid, 0
   * outside, 1 inside. */
  int mulib_cgeom_polygon_contains (const mulib_linalg_vec2_t *points,
                                    size_t count, mulib_linalg_vec2_t point);
  /** @brief Segment intersection, including collinear overlap and zero-length
   * segments; -1 invalid. */
  int mulib_cgeom_segments_intersect (mulib_linalg_vec2_t a,
                                      mulib_linalg_vec2_t b,
                                      mulib_linalg_vec2_t c,
                                      mulib_linalg_vec2_t d);
  /** @brief Closest point on a segment (a for a degenerate segment). Finite
   * inputs required. */
  mulib_linalg_vec3_t mulib_cgeom_closest_segment (mulib_linalg_vec3_t p,
                                                   mulib_linalg_vec3_t a,
                                                   mulib_linalg_vec3_t b);
  /** @brief Barycentric weights of the orthogonal projection; 0 for
   * degenerate/nonfinite input. Output is unchanged on failure. Weights
   * correspond to a,b,c, respectively.
   */
  int mulib_cgeom_barycentric (mulib_cgeom_triangle_t triangle,
                               mulib_linalg_vec3_t point,
                               mulib_linalg_vec3_t *weights);
  /** @brief Bounding box of nonempty finite points; output unchanged on
   * failure. */
  int mulib_cgeom_bounds (const mulib_linalg_vec3_t *points, size_t count,
                          mulib_cgeom_aabb_t *out);
  /** @brief Monotone-chain hull, CCW without repeated endpoint or interior
   * collinear points. Input is copied. count<=1048576. NULL out/capacity=0
   * queries required size. On insufficient capacity return 0 and set *written
   * to the required size; otherwise errors preserve both outputs. Empty input
   * succeeds with size zero.
   */
  int mulib_cgeom_convex_hull (const mulib_linalg_vec2_t *points, size_t count,
                               mulib_linalg_vec2_t *out, size_t capacity,
                               size_t *written);
  /** @brief SIMD squared distances to a reference point. Arrays must not
   * overlap. Float overflow produces infinity; NULL arrays are valid only for
   * count zero.
   */
  int mulib_cgeom_distances_squared (const mulib_linalg_vec3_t *points,
                                     size_t count,
                                     mulib_linalg_vec3_t reference,
                                     float *out);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_CGEOM) && !defined(MULIB_CGEOM_IMPLEMENTED)
#define MULIB_CGEOM_IMPLEMENTED
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_cgeom_vec2_valid_ (mulib_linalg_vec2_t v)
  {
    return isfinite (v.x) && isfinite (v.y);
  }
  int
  mulib_cgeom_vec3_valid (mulib_linalg_vec3_t v)
  {
    return isfinite (v.x) && isfinite (v.y) && isfinite (v.z);
  }
  int
  mulib_cgeom_ray_valid (mulib_cgeom_ray_t r)
  {
    return mulib_cgeom_vec3_valid (r.origin)
           && mulib_cgeom_vec3_valid (r.direction)
           && (r.direction.x != 0 || r.direction.y != 0 || r.direction.z != 0);
  }
  int
  mulib_cgeom_aabb_valid (mulib_cgeom_aabb_t b)
  {
    return mulib_cgeom_vec3_valid (b.min) && mulib_cgeom_vec3_valid (b.max)
           && b.min.x <= b.max.x && b.min.y <= b.max.y && b.min.z <= b.max.z;
  }
  int
  mulib_cgeom_sphere_valid (mulib_cgeom_sphere_t s)
  {
    return mulib_cgeom_vec3_valid (s.center) && isfinite (s.radius)
           && s.radius >= 0;
  }
  double
  mulib_cgeom_orient2 (mulib_linalg_vec2_t a, mulib_linalg_vec2_t b,
                       mulib_linalg_vec2_t c)
  {
    return ((double)b.x - a.x) * ((double)c.y - a.y)
           - ((double)b.y - a.y) * ((double)c.x - a.x);
  }
  double
  mulib_cgeom_polygon_area (const mulib_linalg_vec2_t *p, size_t n)
  {
    double sum = 0;
    if ((!p && n) || n > SIZE_MAX / sizeof (*p))
      return NAN;
    for (size_t i = 0; i < n; ++i)
      if (!mulib_cgeom_vec2_valid_ (p[i]))
        return NAN;
    for (size_t i = 1; i + 1 < n; ++i)
      sum += mulib_cgeom_orient2 (p[0], p[i], p[i + 1]);
    return sum * 0.5;
  }
  static int
  mulib_cgeom_on_segment_ (mulib_linalg_vec2_t a, mulib_linalg_vec2_t b,
                           mulib_linalg_vec2_t p)
  {
    return mulib_cgeom_orient2 (a, b, p) == 0 && p.x >= fminf (a.x, b.x)
           && p.x <= fmaxf (a.x, b.x) && p.y >= fminf (a.y, b.y)
           && p.y <= fmaxf (a.y, b.y);
  }
  int
  mulib_cgeom_polygon_contains (const mulib_linalg_vec2_t *p, size_t n,
                                mulib_linalg_vec2_t q)
  {
    int inside = 0;
    if (!mulib_cgeom_vec2_valid_ (q)
        || !isfinite (mulib_cgeom_polygon_area (p, n)))
      return -1;
    if (n < 3)
      return 0;
    for (size_t i = 0, j = n - 1; i < n; j = i++)
      {
        double side = mulib_cgeom_orient2 (p[j], p[i], q);
        if (mulib_cgeom_on_segment_ (p[j], p[i], q))
          return 1;
        if ((p[i].y > q.y) != (p[j].y > q.y)
            && ((side > 0) == (p[i].y > p[j].y)))
          inside = !inside;
      }
    return inside;
  }
  int
  mulib_cgeom_segments_intersect (mulib_linalg_vec2_t a, mulib_linalg_vec2_t b,
                                  mulib_linalg_vec2_t c, mulib_linalg_vec2_t d)
  {
    double x, y, z, w;
    if (!mulib_cgeom_vec2_valid_ (a) || !mulib_cgeom_vec2_valid_ (b)
        || !mulib_cgeom_vec2_valid_ (c) || !mulib_cgeom_vec2_valid_ (d))
      return -1;
    x = mulib_cgeom_orient2 (a, b, c);
    y = mulib_cgeom_orient2 (a, b, d);
    z = mulib_cgeom_orient2 (c, d, a);
    w = mulib_cgeom_orient2 (c, d, b);
    if (mulib_cgeom_on_segment_ (a, b, c) || mulib_cgeom_on_segment_ (a, b, d)
        || mulib_cgeom_on_segment_ (c, d, a)
        || mulib_cgeom_on_segment_ (c, d, b))
      return 1;
    return ((x > 0 && y < 0) || (x < 0 && y > 0))
           && ((z > 0 && w < 0) || (z < 0 && w > 0));
  }
  mulib_linalg_vec3_t
  mulib_cgeom_closest_segment (mulib_linalg_vec3_t p, mulib_linalg_vec3_t a,
                               mulib_linalg_vec3_t b)
  {
    double x = (double)b.x - a.x, y = (double)b.y - a.y, z = (double)b.z - a.z,
           d = x * x + y * y + z * z;
    double t = d ? (((double)p.x - a.x) * x + ((double)p.y - a.y) * y
                    + ((double)p.z - a.z) * z)
                       / d
                 : 0;
    return mulib_linalg_vec3_lerp (a, b, (float)fmax (0, fmin (1, t)));
  }
  int
  mulib_cgeom_barycentric (mulib_cgeom_triangle_t t, mulib_linalg_vec3_t p,
                           mulib_linalg_vec3_t *out)
  {
    double u[3] = { (double)t.b.x - t.a.x, (double)t.b.y - t.a.y,
                    (double)t.b.z - t.a.z };
    double v[3] = { (double)t.c.x - t.a.x, (double)t.c.y - t.a.y,
                    (double)t.c.z - t.a.z };
    double w[3]
        = { (double)p.x - t.a.x, (double)p.y - t.a.y, (double)p.z - t.a.z };
    double normal[3], d = 0, x = 0, y = 0;
    mulib_linalg_vec3_t r;
    if (!out || !mulib_cgeom_vec3_valid (t.a) || !mulib_cgeom_vec3_valid (t.b)
        || !mulib_cgeom_vec3_valid (t.c) || !mulib_cgeom_vec3_valid (p))
      return 0;
    for (int i = 0; i < 3; ++i)
      {
        int j = (i + 1) % 3, k = (i + 2) % 3;
        normal[i] = u[j] * v[k] - u[k] * v[j];
        d += normal[i] * normal[i];
      }
    if (d <= 0)
      return 0;
    for (int i = 0; i < 3; ++i)
      {
        int j = (i + 1) % 3, k = (i + 2) % 3;
        x += (w[j] * v[k] - w[k] * v[j]) * normal[i];
        y += (u[j] * w[k] - u[k] * w[j]) * normal[i];
      }
    x /= d;
    y /= d;
    r.x = (float)(1 - x - y);
    r.y = (float)x;
    r.z = (float)y;
    if (!mulib_cgeom_vec3_valid (r))
      return 0;
    *out = r;
    return 1;
  }
  int
  mulib_cgeom_bounds (const mulib_linalg_vec3_t *p, size_t n,
                      mulib_cgeom_aabb_t *out)
  {
    mulib_cgeom_aabb_t b;
    if (!out || !p || !n || n > SIZE_MAX / sizeof (*p))
      return 0;
    b.min = b.max = p[0];
    for (size_t i = 0; i < n; ++i)
      {
        if (!mulib_cgeom_vec3_valid (p[i]))
          return 0;
        b.min.x = fminf (b.min.x, p[i].x);
        b.min.y = fminf (b.min.y, p[i].y);
        b.min.z = fminf (b.min.z, p[i].z);
        b.max.x = fmaxf (b.max.x, p[i].x);
        b.max.y = fmaxf (b.max.y, p[i].y);
        b.max.z = fmaxf (b.max.z, p[i].z);
      }
    *out = b;
    return 1;
  }
  static int
  mulib_cgeom_compare_ (const void *a, const void *b)
  {
    const mulib_linalg_vec2_t *p = (const mulib_linalg_vec2_t *)a,
                              *q = (const mulib_linalg_vec2_t *)b;
    if (p->x != q->x)
      return p->x < q->x ? -1 : 1;
    return (p->y > q->y) - (p->y < q->y);
  }
  int
  mulib_cgeom_convex_hull (const mulib_linalg_vec2_t *p, size_t n,
                           mulib_linalg_vec2_t *out, size_t capacity,
                           size_t *written)
  {
    mulib_linalg_vec2_t *copy, *hull;
    size_t unique = 0, k = 0, lower;
    if (!written || (!p && n) || (!out && capacity) || n > 1048576
        || n > SIZE_MAX / (3 * sizeof (*p)))
      return 0;
    for (size_t i = 0; i < n; ++i)
      if (!mulib_cgeom_vec2_valid_ (p[i]))
        return 0;
    if (!n)
      {
        *written = 0;
        return 1;
      }
    copy = (mulib_linalg_vec2_t *)malloc (3 * n * sizeof (*copy));
    if (!copy)
      return 0;
    hull = copy + n;
    memcpy (copy, p, n * sizeof (*p));
    qsort (copy, n, sizeof (*copy), mulib_cgeom_compare_);
    for (size_t i = 0; i < n; ++i)
      if (!unique || mulib_cgeom_compare_ (&copy[i], &copy[unique - 1]))
        copy[unique++] = copy[i];
    for (size_t i = 0; i < unique; ++i)
      {
        while (k >= 2
               && mulib_cgeom_orient2 (hull[k - 2], hull[k - 1], copy[i]) <= 0)
          --k;
        hull[k++] = copy[i];
      }
    lower = k;
    for (size_t i = unique - 1; i > 0; --i)
      {
        while (k > lower
               && mulib_cgeom_orient2 (hull[k - 2], hull[k - 1], copy[i - 1])
                      <= 0)
          --k;
        hull[k++] = copy[i - 1];
      }
    if (unique > 1)
      --k;
    *written = k;
    if (out && capacity >= k)
      memcpy (out, hull, k * sizeof (*out));
    free (copy);
    return !out || capacity >= k;
  }
  int
  mulib_cgeom_distances_squared (const mulib_linalg_vec3_t *p, size_t n,
                                 mulib_linalg_vec3_t q, float *out)
  {
    size_t i = 0;
    if ((!p && n) || (!out && n) || n > SIZE_MAX / sizeof (*p))
      return 0;
    for (; n - i >= 4; i += 4)
      {
        mulib_simdmath_f4_t x, y, z, d;
        for (int k = 0; k < 4; ++k)
          {
            x.lane[k] = p[i + k].x;
            y.lane[k] = p[i + k].y;
            z.lane[k] = p[i + k].z;
          }
        x = mulib_simdmath_sub (x, mulib_simdmath_splat (q.x));
        y = mulib_simdmath_sub (y, mulib_simdmath_splat (q.y));
        z = mulib_simdmath_sub (z, mulib_simdmath_splat (q.z));
        d = mulib_simdmath_add (mulib_simdmath_add (mulib_simdmath_mul (x, x),
                                                    mulib_simdmath_mul (y, y)),
                                mulib_simdmath_mul (z, z));
        mulib_simdmath_store (out + i, d);
      }
    for (; i < n; ++i)
      {
        mulib_linalg_vec3_t d = mulib_linalg_vec3_sub (p[i], q);
        out[i] = mulib_linalg_vec3_dot (d, d);
      }
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
