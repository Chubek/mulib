#define MULIB_IMPL_LINALG
#define MULIB_IMPL_SIMDMATH
#define MULIB_IMPL_CGEOM
#define MULIB_IMPL_COLLISION
#include "mulib-collision.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <string.h>

static int
near (float a, float b)
{
  return fabsf (a - b) < 0.0001f;
}
static void
matrices (void)
{
  mulib_linalg_vec3_t zero = { 0, 0, 0 }, tiny = { FLT_TRUE_MIN, 0, 0 },
                      axis = { 0, 0, 1 }, n = { 9, 8, 7 };
  mulib_linalg_vec3_t translation = { 3, -4, 5 }, scale = { 2, 3, 4 },
                      eye = { 0, 0, 5 }, up = { 0, 1, 0 };
  mulib_linalg_mat4_t m, inv, id, before, view, proj;
  mulib_linalg_vec4_t v = { 1, 2, 3, 1 }, r;
  mulib_linalg_quat_t q, identity = { 0, 0, 0, 1 }, half;
  assert (!mulib_linalg_vec3_normalize (zero, &n) && n.x == 9);
  assert (mulib_linalg_vec3_normalize (tiny, &n) && n.x == 1 && n.y == 0);
  tiny.x = FLT_MAX;
  tiny.y = FLT_MAX;
  assert (mulib_linalg_vec3_normalize (tiny, &n) && near (n.x, 0.70710678f));
  tiny.x = NAN;
  assert (!mulib_linalg_vec3_normalize (tiny, &n));
  m = mulib_linalg_mat4_mul (mulib_linalg_mat4_translation (translation),
                             mulib_linalg_mat4_scaling (scale));
  r = mulib_linalg_mat4_transform (m, v);
  assert (r.x == 5 && r.y == 2 && r.z == 17 && r.w == 1);
  assert (mulib_linalg_mat4_inverse (m, &inv));
  r = mulib_linalg_mat4_transform (inv, r);
  assert (near (r.x, 1) && near (r.y, 2) && near (r.z, 3));
  id = mulib_linalg_mat4_mul (inv, m);
  for (int i = 0; i < 16; ++i)
    assert (near (id.m[i], i % 5 == 0 ? 1 : 0));
  before = inv;
  m = mulib_linalg_mat4_scaling (zero);
  assert (!mulib_linalg_mat4_inverse (m, &inv)
          && !memcmp (&before, &inv, sizeof inv));
  assert (mulib_linalg_look_at (eye, zero, up, &view));
  r.x = 0;
  r.y = 0;
  r.z = 5;
  r.w = 1;
  r = mulib_linalg_mat4_transform (view, r);
  assert (near (r.z, 0));
  assert (!mulib_linalg_look_at (eye, eye, up, &view));
  assert (mulib_linalg_perspective (1.2f, 1.5f, 1, 10, &proj));
  r.x = 0;
  r.y = 0;
  r.z = -1;
  r.w = 1;
  r = mulib_linalg_mat4_transform (proj, r);
  assert (near (r.z / r.w, -1));
  r.x = 0;
  r.y = 0;
  r.z = -10;
  r.w = 1;
  r = mulib_linalg_mat4_transform (proj, r);
  assert (near (r.z / r.w, 1));
  assert (!mulib_linalg_perspective (0, 1, 1, 10, &proj));
  assert (!mulib_linalg_perspective (1, 0, 1, 10, &proj));
  assert (mulib_linalg_ortho (-2, 2, -1, 1, 1, 10, &proj));
  r.x = 2;
  r.y = 1;
  r.z = -10;
  r.w = 1;
  r = mulib_linalg_mat4_transform (proj, r);
  assert (near (r.x, 1) && near (r.y, 1) && near (r.z, 1));
  assert (mulib_linalg_quat_axis_angle (axis, 1.57079632679f, &q));
  assert (mulib_linalg_quat_matrix (q, &m));
  r.x = 1;
  r.y = 0;
  r.z = 0;
  r.w = 1;
  r = mulib_linalg_mat4_transform (m, r);
  assert (near (r.x, 0) && near (r.y, 1));
  assert (mulib_linalg_quat_slerp (identity, q, 0.5f, &half));
  assert (mulib_linalg_quat_matrix (half, &m));
  r.x = 1;
  r.y = 0;
  r.z = 0;
  r.w = 1;
  r = mulib_linalg_mat4_transform (m, r);
  assert (near (r.x, 0.70710678f) && near (r.y, r.x));
  identity.w = -1;
  assert (mulib_linalg_quat_slerp (identity, q, 0.5f, &half));
  assert (!mulib_linalg_quat_slerp (identity, q, NAN, &half));
}
static void
simd (void)
{
  float a[5] = { 99, 1, 4, 9, 16 }, b[4];
  mulib_simdmath_f4_t x = mulib_simdmath_load (a + 1), y;
  mulib_linalg_vec3_t shift = { 4, 5, 6 };
  mulib_linalg_mat4_t m = mulib_linalg_mat4_translation (shift);
  mulib_linalg_vec4_t v[5] = { { 1, 2, 3, 1 },
                               { -1, 4, 0, 1 },
                               { 0, 0, 0, 1 },
                               { 1, 1, 1, 0 },
                               { 2, 3, 4, 1 } },
                      original[5];
  assert (mulib_simdmath_backend ());
#ifdef MULIB_SIMDMATH_SCALAR
  assert (!strcmp (mulib_simdmath_backend (), "scalar"));
#endif
  y = mulib_simdmath_sqrt (x);
  mulib_simdmath_store (b, y);
  for (int i = 0; i < 4; ++i)
    assert (b[i] == i + 1);
  y = mulib_simdmath_div (mulib_simdmath_mul (x, x), x);
  mulib_simdmath_store (b, y);
  for (int i = 0; i < 4; ++i)
    assert (b[i] == a[i + 1]);
  assert (mulib_simdmath_dot (x, mulib_simdmath_splat (1)) == 30);
  assert (mulib_simdmath_normalize (mulib_simdmath_splat (FLT_MAX), &y));
  for (int i = 0; i < 4; ++i)
    assert (y.lane[i] == 0.5f);
  assert (!mulib_simdmath_normalize (mulib_simdmath_splat (0), &y));
  {
    mulib_linalg_mat4_t product = mulib_simdmath_mat4_mul (m, m);
    assert (product.m[12] == 8 && product.m[13] == 10 && product.m[14] == 12);
  }
  memcpy (original, v, sizeof v);
  assert (mulib_simdmath_transform_many (m, v, v, 5));
  for (int i = 0; i < 5; ++i)
    {
      mulib_linalg_vec4_t expected
          = mulib_linalg_mat4_transform (m, original[i]);
      assert (!memcmp (&expected, &v[i], sizeof expected));
    }
  assert (mulib_simdmath_transform_many (m, NULL, NULL, 0));
  assert (!mulib_simdmath_transform_many (m, NULL, v, 1));
}
static void
geometry (void)
{
  mulib_linalg_vec2_t p[] = { { 1, 1 }, { 0, 0 },    { 1, 0 },      { 0, 1 },
                              { 1, 1 }, { 0.5f, 0 }, { 0.5f, 0.5f } },
                      h[8], before[8];
  size_t n = 99;
  mulib_linalg_vec2_t q = { 0.5f, 0.5f }, a = { 0, 0 }, b = { 1, 0 },
                      c = { 2, 0 }, d = { 3, 0 };
  mulib_cgeom_triangle_t t = { { 0, 0, 0 }, { 2, 0, 0 }, { 0, 2, 0 } };
  mulib_linalg_vec3_t v = { 0.5f, 0.5f, 7 }, weights;
  mulib_linalg_vec3_t points[]
      = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 2, 0 }, { 0, 0, 3 }, { 1, 2, 2 } };
  float distances[5];
  mulib_cgeom_aabb_t bounds;
  assert (mulib_cgeom_convex_hull (p, 7, h, 8, &n) && n == 4
          && mulib_cgeom_polygon_area (h, n) == 1);
  assert (mulib_cgeom_polygon_contains (h, n, q) == 1);
  q.x = 2;
  assert (mulib_cgeom_polygon_contains (h, n, q) == 0);
  q.x = 0;
  assert (mulib_cgeom_polygon_contains (h, n, q) == 1);
  memcpy (before, h, sizeof h);
  assert (!mulib_cgeom_convex_hull (p, 7, h, 3, &n) && n == 4
          && !memcmp (before, h, sizeof h));
  assert (mulib_cgeom_convex_hull (p, 7, NULL, 0, &n) && n == 4);
  assert (mulib_cgeom_convex_hull (NULL, 0, NULL, 0, &n) && n == 0);
  p[0].x = NAN;
  n = 99;
  assert (!mulib_cgeom_convex_hull (p, 7, h, 8, &n) && n == 99);
  assert (!mulib_cgeom_segments_intersect (a, b, c, d));
  c.x = 1;
  assert (mulib_cgeom_segments_intersect (a, b, c, d) == 1);
  assert (!mulib_cgeom_segments_intersect (a, a, b, b));
  assert (mulib_cgeom_barycentric (t, v, &weights) && weights.x == 0.5f
          && weights.y == 0.25f && weights.z == 0.25f);
  t.c = t.a;
  assert (!mulib_cgeom_barycentric (t, v, &weights));
  v = mulib_cgeom_closest_segment (v, points[0], points[1]);
  assert (v.x == 0.5f && v.y == 0 && v.z == 0);
  assert (mulib_cgeom_bounds (points, 5, &bounds) && bounds.max.x == 1
          && bounds.max.y == 2 && bounds.max.z == 3);
  assert (mulib_cgeom_distances_squared (points, 5, points[0], distances));
  assert (distances[0] == 0 && distances[1] == 1 && distances[2] == 4
          && distances[3] == 9 && distances[4] == 9);
}
static void
collisions (void)
{
  mulib_cgeom_sphere_t a = { { 0, 0, 0 }, 1 }, b = { { 2, 0, 0 }, 1 };
  mulib_collision_contact_t contact;
  mulib_cgeom_aabb_t box = { { -1, -1, -1 }, { 1, 1, 1 } },
                     outer = { { -2, -2, -2 }, { 2, 2, 2 } };
  mulib_cgeom_ray_t r = { { 0, 0, 3 }, { 0, 0, -2 } };
  mulib_collision_hit_t hit, before;
  mulib_cgeom_triangle_t t = { { -1, -1, 0 }, { 1, -1, 0 }, { 0, 1, 0 } };
  mulib_linalg_vec2_t pa[] = { { 0, 0 }, { 2, 0 }, { 1, 2 } },
                      pb[] = { { 2, 0 }, { 4, 0 }, { 3, 2 } };
  assert (mulib_collision_spheres (a, b, &contact) == 1 && contact.depth == 0
          && contact.normal.x == 1);
  b.center.x = 2.01f;
  assert (!mulib_collision_spheres (a, b, NULL));
  b.radius = -1;
  assert (mulib_collision_spheres (a, b, NULL) == -1);
  assert (mulib_collision_aabbs (box, outer, &contact) == 1
          && contact.depth == 3);
  assert (mulib_collision_sphere_aabb (a, box, &contact) == 1
          && contact.depth == 2);
  a.center.x = 2;
  assert (mulib_collision_sphere_aabb (a, box, &contact) == 1
          && contact.depth == 0 && contact.normal.x == -1);
  a.center.x = 0;
  assert (mulib_collision_ray_sphere (r, a, 0, 10, &hit) == 1 && hit.t == 1
          && hit.point.z == 1 && hit.normal.z == 1);
  assert (mulib_collision_ray_sphere (r, a, 1.5f, 10, &hit) == 1
          && hit.t == 2);
  before = hit;
  assert (!mulib_collision_ray_sphere (r, a, 0, 0.5f, &hit)
          && !memcmp (&before, &hit, sizeof hit));
  r.origin.x = 1;
  assert (mulib_collision_ray_sphere (r, a, 0, 10, &hit) == 1
          && hit.t == 1.5f);
  r.origin.x = 0;
  assert (mulib_collision_ray_aabb (r, box, 0, 10, &hit) == 1 && hit.t == 1
          && hit.normal.z == 1);
  r.origin.z = 0;
  assert (mulib_collision_ray_aabb (r, box, 0, 10, &hit) == 1 && hit.t == 0.5f
          && hit.normal.z == -1);
  r.origin.x = 2;
  assert (!mulib_collision_ray_aabb (r, box, 0, 10, &hit));
  r.origin.x = 0;
  r.origin.z = 3;
  assert (mulib_collision_ray_triangle (r, t, 0, 10, &hit) == 1
          && hit.t == 1.5f && hit.normal.z == 1);
  assert (hit.barycentric.x == 0.25f && hit.barycentric.y == 0.25f
          && hit.barycentric.z == 0.5f);
  r.direction.z = 0;
  assert (mulib_collision_ray_triangle (r, t, 0, 10, &hit) == -1);
  assert (mulib_collision_convex2 (pa, 3, pb, 3) == 1);
  for (int i = 0; i < 3; ++i)
    pb[i].x += 0.01f;
  assert (!mulib_collision_convex2 (pa, 3, pb, 3));
  /* A distant parallel ray outside the silhouette must not become tangent
     through cancellation in the quadratic discriminant. */
  r.origin.x = 2;
  r.origin.z = 100000000;
  r.direction.z = -1;
  assert (mulib_collision_ray_sphere (r, a, 0, FLT_MAX, &hit) == 0);
  {
    mulib_cgeom_triangle_t thin = { {0,0,0}, {1,0,0}, {1,1e-12f,0} };
    mulib_linalg_vec3_t point = {0.75f, 0.25e-12f, 3}, weights;
    assert (mulib_cgeom_barycentric (thin, point, &weights));
    assert (near (weights.x, 0.25f) && near (weights.y, 0.5f)
            && near (weights.z, 0.25f));
  }
}
int
main (void)
{
  matrices ();
  simd ();
  geometry ();
  collisions ();
  return 0;
}
