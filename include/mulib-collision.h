/**
 * @file mulib-collision.h
 * @brief Primitive overlap contacts, convex SAT and ray intersection queries.
 * Define MULIB_IMPL_COLLISION and the cgeom/linalg/simdmath dependencies.
 * Queries return -1 invalid, 0 miss, 1 hit. Touching counts as collision.
 * Optional outputs are unchanged on miss/error. Rays need not be normalized.
 */
#ifndef MULIB_COLLISION_H
#define MULIB_COLLISION_H
#include "mulib-cgeom.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Unit normal: moving the first object by -normal*depth separates
   * it. For ambiguous coincident objects a deterministic axis is selected.
   */
  typedef struct mulib_collision_contact
  {
    mulib_linalg_vec3_t normal;
    float depth;
  } mulib_collision_contact_t;
  /** @brief Ray parameter, outward/geometric unit normal and barycentric
   * weights (triangles only). */
  typedef struct mulib_collision_hit
  {
    float t;
    mulib_linalg_vec3_t point, normal, barycentric;
  } mulib_collision_hit_t;
  /** @brief Sphere/sphere overlap, including contained and concentric spheres.
   */
  int mulib_collision_spheres (mulib_cgeom_sphere_t a, mulib_cgeom_sphere_t b,
                               mulib_collision_contact_t *out);
  /** @brief Axis-aligned box overlap and minimum separating translation. */
  int mulib_collision_aabbs (mulib_cgeom_aabb_t a, mulib_cgeom_aabb_t b,
                             mulib_collision_contact_t *out);
  /** @brief Sphere/box overlap; handles sphere centers inside the box. */
  int mulib_collision_sphere_aabb (mulib_cgeom_sphere_t sphere,
                                   mulib_cgeom_aabb_t box,
                                   mulib_collision_contact_t *out);
  /** @brief Separating-axis test. Inputs must be ordered convex
   * polygons, 3..4096 vertices each. */
  int mulib_collision_convex2 (const mulib_linalg_vec2_t *a, size_t na,
                               const mulib_linalg_vec2_t *b, size_t nb);
  /** @brief Nearest sphere surface in finite inclusive [t_min,t_max],
   * t_min>=0. */
  int mulib_collision_ray_sphere (mulib_cgeom_ray_t ray,
                                  mulib_cgeom_sphere_t sphere, float t_min,
                                  float t_max, mulib_collision_hit_t *out);
  /** @brief Nearest box surface, including exit surface when ray starts
   * inside. */
  int mulib_collision_ray_aabb (mulib_cgeom_ray_t ray, mulib_cgeom_aabb_t box,
                                float t_min, float t_max,
                                mulib_collision_hit_t *out);
  /** @brief Two-sided triangle test; degenerate or parallel triangles miss. */
  int mulib_collision_ray_triangle (mulib_cgeom_ray_t ray,
                                    mulib_cgeom_triangle_t triangle,
                                    float t_min, float t_max,
                                    mulib_collision_hit_t *out);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_COLLISION) && !defined(MULIB_COLLISION_IMPLEMENTED)
#define MULIB_COLLISION_IMPLEMENTED
#include <float.h>
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static mulib_linalg_vec3_t
  mulib_collision_axis_ (int axis, float sign)
  {
    mulib_linalg_vec3_t r = { 0, 0, 0 };
    if (axis == 0)
      r.x = sign;
    else if (axis == 1)
      r.y = sign;
    else
      r.z = sign;
    return r;
  }
  static int
  mulib_collision_contact_ (double depth, mulib_linalg_vec3_t n,
                            mulib_collision_contact_t *out)
  {
    if (depth > FLT_MAX)
      return -1;
    if (out)
      {
        out->normal = n;
        out->depth = (float)depth;
      }
    return 1;
  }
  int
  mulib_collision_spheres (mulib_cgeom_sphere_t a, mulib_cgeom_sphere_t b,
                           mulib_collision_contact_t *out)
  {
    double x = (double)b.center.x - a.center.x,
           y = (double)b.center.y - a.center.y,
           z = (double)b.center.z - a.center.z;
    double d = hypot (hypot (x, y), z), r = (double)a.radius + b.radius;
    mulib_linalg_vec3_t n = { 1, 0, 0 };
    if (!mulib_cgeom_sphere_valid (a) || !mulib_cgeom_sphere_valid (b))
      return -1;
    if (d > r)
      return 0;
    if (d)
      {
        n.x = (float)(x / d);
        n.y = (float)(y / d);
        n.z = (float)(z / d);
      }
    return mulib_collision_contact_ (r - d, n, out);
  }
  int
  mulib_collision_aabbs (mulib_cgeom_aabb_t a, mulib_cgeom_aabb_t b,
                         mulib_collision_contact_t *out)
  {
    double amin[3] = { a.min.x, a.min.y, a.min.z },
           amax[3] = { a.max.x, a.max.y, a.max.z };
    double bmin[3] = { b.min.x, b.min.y, b.min.z },
           bmax[3] = { b.max.x, b.max.y, b.max.z };
    double best = DBL_MAX;
    int axis = 0;
    float sign = 1;
    if (!mulib_cgeom_aabb_valid (a) || !mulib_cgeom_aabb_valid (b))
      return -1;
    for (int i = 0; i < 3; ++i)
      {
        double left = amax[i] - bmin[i], right = bmax[i] - amin[i];
        if (left < 0 || right < 0)
          return 0;
        if (left < best)
          {
            best = left;
            axis = i;
            sign = 1;
          }
        if (right < best)
          {
            best = right;
            axis = i;
            sign = -1;
          }
      }
    return mulib_collision_contact_ (best, mulib_collision_axis_ (axis, sign),
                                     out);
  }
  int
  mulib_collision_sphere_aabb (mulib_cgeom_sphere_t s, mulib_cgeom_aabb_t b,
                               mulib_collision_contact_t *out)
  {
    double p[3] = { s.center.x, s.center.y, s.center.z },
           lo[3] = { b.min.x, b.min.y, b.min.z },
           hi[3] = { b.max.x, b.max.y, b.max.z };
    double v[3], d, best = DBL_MAX;
    int axis = 0;
    float sign = 1;
    mulib_linalg_vec3_t n;
    if (!mulib_cgeom_sphere_valid (s) || !mulib_cgeom_aabb_valid (b))
      return -1;
    for (int i = 0; i < 3; ++i)
      v[i] = fmax (lo[i], fmin (hi[i], p[i])) - p[i];
    d = hypot (hypot (v[0], v[1]), v[2]);
    if (d > s.radius)
      return 0;
    if (d)
      {
        n.x = (float)(v[0] / d);
        n.y = (float)(v[1] / d);
        n.z = (float)(v[2] / d);
        return mulib_collision_contact_ (s.radius - d, n, out);
      }
    for (int i = 0; i < 3; ++i)
      {
        if (p[i] - lo[i] < best)
          {
            best = p[i] - lo[i];
            axis = i;
            sign = 1;
          }
        if (hi[i] - p[i] < best)
          {
            best = hi[i] - p[i];
            axis = i;
            sign = -1;
          }
      }
    return mulib_collision_contact_ (s.radius + best,
                                     mulib_collision_axis_ (axis, sign), out);
  }
  int
  mulib_collision_convex2 (const mulib_linalg_vec2_t *a, size_t na,
                           const mulib_linalg_vec2_t *b, size_t nb)
  {
    double aa, ba;
    if (na < 3 || nb < 3 || na > 4096 || nb > 4096)
      return -1;
    aa = mulib_cgeom_polygon_area (a, na);
    ba = mulib_cgeom_polygon_area (b, nb);
    if (!isfinite (aa) || !isfinite (ba) || aa == 0 || ba == 0)
      return -1;
    for (int pass = 0; pass < 2; ++pass)
      {
        const mulib_linalg_vec2_t *p = pass ? b : a;
        size_t n = pass ? nb : na;
        for (size_t i = 0; i < n; ++i)
          {
            size_t j = (i + 1) % n;
            double x = (double)p[j].y - p[i].y, y = (double)p[i].x - p[j].x;
            double al = DBL_MAX, ah = -DBL_MAX, bl = DBL_MAX, bh = -DBL_MAX;
            for (size_t k = 0; k < na; ++k)
              {
                double d = a[k].x * x + a[k].y * y;
                al = fmin (al, d);
                ah = fmax (ah, d);
              }
            for (size_t k = 0; k < nb; ++k)
              {
                double d = b[k].x * x + b[k].y * y;
                bl = fmin (bl, d);
                bh = fmax (bh, d);
              }
            if (ah < bl || bh < al)
              return 0;
          }
      }
    return 1;
  }
  static int
  mulib_collision_ray_valid_ (mulib_cgeom_ray_t r, float lo, float hi)
  {
    return mulib_cgeom_ray_valid (r) && isfinite (lo) && isfinite (hi)
           && lo >= 0 && hi >= lo;
  }
  static int
  mulib_collision_hit_ (mulib_cgeom_ray_t r, double t, mulib_linalg_vec3_t n,
                        mulib_linalg_vec3_t bary, mulib_collision_hit_t *out)
  {
    mulib_collision_hit_t h;
    h.t = (float)t;
    h.normal = n;
    h.barycentric = bary;
    h.point.x = (float)(r.origin.x + t * r.direction.x);
    h.point.y = (float)(r.origin.y + t * r.direction.y);
    h.point.z = (float)(r.origin.z + t * r.direction.z);
    if (!mulib_cgeom_vec3_valid (h.point))
      return -1;
    if (out)
      *out = h;
    return 1;
  }
  int
  mulib_collision_ray_sphere (mulib_cgeom_ray_t r, mulib_cgeom_sphere_t s,
                              float lo, float hi, mulib_collision_hit_t *out)
  {
    double x = (double)r.origin.x - s.center.x,
           y = (double)r.origin.y - s.center.y,
           z = (double)r.origin.z - s.center.z;
    double dx = r.direction.x, dy = r.direction.y, dz = r.direction.z,
           a = dx * dx + dy * dy + dz * dz;
    double b = x * dx + y * dy + z * dz,
           c = x * x + y * y + z * z - (double)s.radius * s.radius,
           d, t, q, t0, t1;
    mulib_linalg_vec3_t n = { 1, 0, 0 }, bary = { 0, 0, 0 };
    if (!mulib_collision_ray_valid_ (r, lo, hi)
        || !mulib_cgeom_sphere_valid (s))
      return -1;
    /* Lagrange's identity avoids subtracting two enormous, nearly equal
       longitudinal distances for distant rays. */
    {
      double cx = y * dz - z * dy, cy = z * dx - x * dz,
             cz = x * dy - y * dx;
      d = a * (double)s.radius * s.radius
          - (cx * cx + cy * cy + cz * cz);
    }
    if (d < 0)
      return 0;
    q = -b - copysign (sqrt (d), b);
    t0 = q / a;
    t1 = q ? c / q : t0;
    if (t0 > t1)
      {
        t = t0;
        t0 = t1;
        t1 = t;
      }
    t = t0 >= lo ? t0 : t1;
    if (t < lo || t > hi)
      return 0;
    if (s.radius > 0)
      {
        double nx = x + t * dx, ny = y + t * dy, nz = z + t * dz,
               len = hypot (hypot (nx, ny), nz);
        if (len)
          {
            n.x = (float)(nx / len);
            n.y = (float)(ny / len);
            n.z = (float)(nz / len);
          }
      }
    return mulib_collision_hit_ (r, t, n, bary, out);
  }
  int
  mulib_collision_ray_aabb (mulib_cgeom_ray_t r, mulib_cgeom_aabb_t b,
                            float lo, float hi, mulib_collision_hit_t *out)
  {
    double o[3] = { r.origin.x, r.origin.y, r.origin.z },
           d[3] = { r.direction.x, r.direction.y, r.direction.z };
    double mn[3] = { b.min.x, b.min.y, b.min.z },
           mx[3] = { b.max.x, b.max.y, b.max.z };
    double enter = -DBL_MAX, leave = DBL_MAX;
    int ia = 0, ib = 0;
    float sa = 1, sb = 1;
    mulib_linalg_vec3_t bary = { 0, 0, 0 };
    if (!mulib_collision_ray_valid_ (r, lo, hi) || !mulib_cgeom_aabb_valid (b))
      return -1;
    for (int i = 0; i < 3; ++i)
      {
        double a, z;
        float s = -1;
        if (d[i] == 0)
          {
            if (o[i] < mn[i] || o[i] > mx[i])
              return 0;
            continue;
          }
        a = (mn[i] - o[i]) / d[i];
        z = (mx[i] - o[i]) / d[i];
        if (a > z)
          {
            double tmp = a;
            a = z;
            z = tmp;
            s = 1;
          }
        if (a > enter)
          {
            enter = a;
            ia = i;
            sa = s;
          }
        if (z < leave)
          {
            leave = z;
            ib = i;
            sb = -s;
          }
        if (enter > leave)
          return 0;
      }
    if (enter >= lo && enter <= hi)
      return mulib_collision_hit_ (r, enter, mulib_collision_axis_ (ia, sa),
                                   bary, out);
    if (leave >= lo && leave <= hi)
      return mulib_collision_hit_ (r, leave, mulib_collision_axis_ (ib, sb),
                                   bary, out);
    return 0;
  }
  int
  mulib_collision_ray_triangle (mulib_cgeom_ray_t r,
                                mulib_cgeom_triangle_t tri, float lo, float hi,
                                mulib_collision_hit_t *out)
  {
    double a[3] = { (double)tri.b.x - tri.a.x, (double)tri.b.y - tri.a.y,
                    (double)tri.b.z - tri.a.z };
    double b[3] = { (double)tri.c.x - tri.a.x, (double)tri.c.y - tri.a.y,
                    (double)tri.c.z - tri.a.z };
    double d[3] = { r.direction.x, r.direction.y, r.direction.z };
    double v[3] = { (double)r.origin.x - tri.a.x, (double)r.origin.y - tri.a.y,
                    (double)r.origin.z - tri.a.z };
    double p[3], q[3], normal[3], det, u, w, t, len;
    mulib_linalg_vec3_t n, bary;
    if (!mulib_collision_ray_valid_ (r, lo, hi)
        || !mulib_cgeom_vec3_valid (tri.a) || !mulib_cgeom_vec3_valid (tri.b)
        || !mulib_cgeom_vec3_valid (tri.c))
      return -1;
    for (int i = 0; i < 3; ++i)
      {
        int j = (i + 1) % 3, k = (i + 2) % 3;
        p[i] = d[j] * b[k] - d[k] * b[j];
        q[i] = v[j] * a[k] - v[k] * a[j];
        normal[i] = a[j] * b[k] - a[k] * b[j];
      }
    det = a[0] * p[0] + a[1] * p[1] + a[2] * p[2];
    if (det == 0)
      return 0;
    u = (v[0] * p[0] + v[1] * p[1] + v[2] * p[2]) / det;
    w = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) / det;
    if (u < 0 || w < 0 || u + w > 1)
      return 0;
    t = (b[0] * q[0] + b[1] * q[1] + b[2] * q[2]) / det;
    if (t < lo || t > hi)
      return 0;
    len = hypot (hypot (normal[0], normal[1]), normal[2]);
    if (!len)
      return 0;
    n.x = (float)(normal[0] / len);
    n.y = (float)(normal[1] / len);
    n.z = (float)(normal[2] / len);
    bary.x = (float)(1 - u - w);
    bary.y = (float)u;
    bary.z = (float)w;
    return mulib_collision_hit_ (r, t, n, bary, out);
  }
#ifdef __cplusplus
}
#endif
#endif
