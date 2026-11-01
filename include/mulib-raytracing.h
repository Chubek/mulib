/**
 * @file mulib-raytracing.h
 * @brief Deterministic CPU ray renderer with shadows, diffuse light and
 * reflections. Define MULIB_IMPL_RAYTRACING plus COLLISION, CGEOM, SIMDMATH,
 * LINALG, GLX and DRM.
 */
#ifndef MULIB_RAYTRACING_H
#define MULIB_RAYTRACING_H
#include "mulib-collision.h"
#include "mulib-glx.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_raytracing_material
  {
    mulib_linalg_vec3_t diffuse, emission;
    float reflectivity;
  } mulib_raytracing_material_t;
  typedef enum mulib_raytracing_kind
  {
    MULIB_RAYTRACING_SPHERE,
    MULIB_RAYTRACING_TRIANGLE
  } mulib_raytracing_kind_t;
  typedef struct mulib_raytracing_object
  {
    mulib_raytracing_kind_t kind;
    mulib_raytracing_material_t material;
    union
    {
      mulib_cgeom_sphere_t sphere;
      mulib_cgeom_triangle_t triangle;
    } shape;
  } mulib_raytracing_object_t;
  typedef struct mulib_raytracing_light
  {
    mulib_linalg_vec3_t position, color;
    float intensity;
  } mulib_raytracing_light_t;
  /** @brief Borrowed immutable arrays; <=65536 objects, <=64 point lights.
   * Coordinates/radii <=1e6 in magnitude. Diffuse/reflectivity/color in [0,1],
   * emission/background/ambient in [0,1000], intensity in [0,1e6].
   */
  typedef struct mulib_raytracing_scene
  {
    const mulib_raytracing_object_t *objects;
    size_t object_count;
    const mulib_raytracing_light_t *lights;
    size_t light_count;
    mulib_linalg_vec3_t ambient, background;
  } mulib_raytracing_scene_t;
  typedef struct mulib_raytracing_camera
  {
    mulib_linalg_vec3_t eye, target, up;
    float fovy;
  } mulib_raytracing_camera_t;
  typedef struct mulib_raytracing_hit
  {
    size_t object;
    mulib_collision_hit_t geometry;
    int front_face;
  } mulib_raytracing_hit_t;
  /** @brief Validate a complete scene without allocation. */
  int mulib_raytracing_scene_valid (const mulib_raytracing_scene_t *scene);
  /** @brief Closest surface in inclusive [min,max]; -1 invalid, 0 miss, 1 hit.
   * Ray parameterization is preserved. Optional output unchanged on
   * miss/error.
   */
  int mulib_raytracing_intersect (const mulib_raytracing_scene_t *scene,
                                  mulib_cgeom_ray_t ray, float t_min,
                                  float t_max, mulib_raytracing_hit_t *out);
  /** @brief Evaluate a normalized ray in linear RGB, with 0..8 reflection
   * bounces. Return 0 for invalid input without changing output. No stochastic
   * sampling.
   */
  int mulib_raytracing_sample (const mulib_raytracing_scene_t *scene,
                               mulib_cgeom_ray_t ray, unsigned bounces,
                               mulib_linalg_vec3_t *out);
  /** @brief Pinhole render into a GLX surface, one center sample/pixel,
   * clamped sRGB. Camera fovy is radians in (0,pi). Validation occurs before
   * drawing. Honors the GLX scissor; overwrites covered pixels. Returns 0 for
   * invalid input, 1 otherwise.
   */
  int mulib_raytracing_render (const mulib_raytracing_scene_t *scene,
                               mulib_raytracing_camera_t camera,
                               unsigned bounces, mulib_glx_t *surface);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_RAYTRACING) && !defined(MULIB_RAYTRACING_IMPLEMENTED)
#define MULIB_RAYTRACING_IMPLEMENTED
#include <float.h>
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_raytracing_color_ (mulib_linalg_vec3_t v, float max)
  {
    return mulib_cgeom_vec3_valid (v) && v.x >= 0 && v.y >= 0 && v.z >= 0
           && v.x <= max && v.y <= max && v.z <= max;
  }
  static int
  mulib_raytracing_point_ (mulib_linalg_vec3_t v)
  {
    return mulib_cgeom_vec3_valid (v) && fabsf (v.x) <= 1000000
           && fabsf (v.y) <= 1000000 && fabsf (v.z) <= 1000000;
  }
  int
  mulib_raytracing_scene_valid (const mulib_raytracing_scene_t *s)
  {
    if (!s || (!s->objects && s->object_count)
        || (!s->lights && s->light_count) || s->object_count > 65536
        || s->light_count > 64 || !mulib_raytracing_color_ (s->ambient, 1000)
        || !mulib_raytracing_color_ (s->background, 1000))
      return 0;
    for (size_t i = 0; i < s->object_count; ++i)
      {
        const mulib_raytracing_object_t *o = &s->objects[i];
        const mulib_raytracing_material_t *m = &o->material;
        if (!mulib_raytracing_color_ (m->diffuse, 1)
            || !mulib_raytracing_color_ (m->emission, 1000)
            || !isfinite (m->reflectivity) || m->reflectivity < 0
            || m->reflectivity > 1)
          return 0;
        if (o->kind == MULIB_RAYTRACING_SPHERE)
          {
            if (!mulib_cgeom_sphere_valid (o->shape.sphere)
                || o->shape.sphere.radius <= 0
                || o->shape.sphere.radius > 1000000
                || !mulib_raytracing_point_ (o->shape.sphere.center))
              return 0;
          }
        else if (o->kind == MULIB_RAYTRACING_TRIANGLE)
          {
            if (!mulib_raytracing_point_ (o->shape.triangle.a)
                || !mulib_raytracing_point_ (o->shape.triangle.b)
                || !mulib_raytracing_point_ (o->shape.triangle.c))
              return 0;
          }
        else
          return 0;
      }
    for (size_t i = 0; i < s->light_count; ++i)
      if (!mulib_raytracing_point_ (s->lights[i].position)
          || !mulib_raytracing_color_ (s->lights[i].color, 1)
          || !isfinite (s->lights[i].intensity) || s->lights[i].intensity < 0
          || s->lights[i].intensity > 1000000)
        return 0;
    return 1;
  }
  static int
  mulib_raytracing_intersect_ (const mulib_raytracing_scene_t *s,
                               mulib_cgeom_ray_t ray, float lo, float hi,
                               mulib_raytracing_hit_t *out)
  {
    int found = 0;
    mulib_raytracing_hit_t result;
    for (size_t i = 0; i < s->object_count; ++i)
      {
        const mulib_raytracing_object_t *o = &s->objects[i];
        mulib_collision_hit_t hit;
        int rc = o->kind == MULIB_RAYTRACING_SPHERE
                     ? mulib_collision_ray_sphere (ray, o->shape.sphere, lo,
                                                   hi, &hit)
                     : mulib_collision_ray_triangle (ray, o->shape.triangle,
                                                     lo, hi, &hit);
        if (rc < 0)
          return -1;
        if (rc && (!found || hit.t < hi))
          {
            found = 1;
            hi = hit.t;
            result.object = i;
            result.geometry = hit;
            result.front_face
                = mulib_linalg_vec3_dot (ray.direction, hit.normal) < 0;
          }
      }
    if (found && out)
      *out = result;
    return found;
  }
  int
  mulib_raytracing_intersect (const mulib_raytracing_scene_t *s,
                              mulib_cgeom_ray_t r, float lo, float hi,
                              mulib_raytracing_hit_t *out)
  {
    if (!mulib_raytracing_scene_valid (s) || !mulib_cgeom_ray_valid (r)
        || !isfinite (lo) || !isfinite (hi) || lo < 0 || hi < lo)
      return -1;
    return mulib_raytracing_intersect_ (s, r, lo, hi, out);
  }
  static mulib_linalg_vec3_t
  mulib_raytracing_product_ (mulib_linalg_vec3_t a, mulib_linalg_vec3_t b)
  {
    mulib_linalg_vec3_t v = { a.x * b.x, a.y * b.y, a.z * b.z };
    return v;
  }
  static mulib_linalg_vec3_t
  mulib_raytracing_trace_ (const mulib_raytracing_scene_t *s,
                           mulib_cgeom_ray_t ray, unsigned depth)
  {
    mulib_raytracing_hit_t hit;
    mulib_linalg_vec3_t color, n, origin;
    float epsilon;
    const mulib_raytracing_material_t *material;
    if (mulib_raytracing_intersect_ (s, ray, 0.0001f, FLT_MAX, &hit) != 1)
      return s->background;
    material = &s->objects[hit.object].material;
    n = hit.geometry.normal;
    if (!hit.front_face)
      n = mulib_linalg_vec3_scale (n, -1);
    epsilon
        = fmaxf (0.0001f, 8 * FLT_EPSILON
                              * fmaxf (fabsf (hit.geometry.point.x),
                                       fmaxf (fabsf (hit.geometry.point.y),
                                              fabsf (hit.geometry.point.z))));
    origin = mulib_linalg_vec3_add (hit.geometry.point,
                                    mulib_linalg_vec3_scale (n, epsilon));
    color = mulib_raytracing_product_ (material->diffuse, s->ambient);
    for (size_t i = 0; i < s->light_count; ++i)
      {
        mulib_cgeom_ray_t shadow;
        float distance, cosine;
        mulib_linalg_vec3_t contribution;
        shadow.origin = origin;
        shadow.direction
            = mulib_linalg_vec3_sub (s->lights[i].position, origin);
        distance = mulib_linalg_vec3_length (shadow.direction);
        if (distance <= 2 * epsilon
            || !mulib_linalg_vec3_normalize (shadow.direction,
                                             &shadow.direction))
          continue;
        cosine = mulib_linalg_vec3_dot (n, shadow.direction);
        if (cosine <= 0
            || mulib_raytracing_intersect_ (s, shadow, epsilon,
                                            distance - epsilon, NULL))
          continue;
        contribution = mulib_raytracing_product_ (material->diffuse,
                                                  s->lights[i].color);
        contribution = mulib_linalg_vec3_scale (contribution,
                                                cosine * s->lights[i].intensity
                                                    / (distance * distance));
        color = mulib_linalg_vec3_add (color, contribution);
      }
    if (depth && material->reflectivity > 0)
      {
        mulib_cgeom_ray_t bounce;
        mulib_linalg_vec3_t reflected;
        bounce.origin = origin;
        bounce.direction = mulib_linalg_vec3_sub (
            ray.direction,
            mulib_linalg_vec3_scale (
                n, 2 * mulib_linalg_vec3_dot (ray.direction, n)));
        (void)mulib_linalg_vec3_normalize (bounce.direction,
                                           &bounce.direction);
        reflected = mulib_raytracing_trace_ (s, bounce, depth - 1);
        color = mulib_linalg_vec3_lerp (color, reflected,
                                        material->reflectivity);
      }
    return mulib_linalg_vec3_add (color, material->emission);
  }
  int
  mulib_raytracing_sample (const mulib_raytracing_scene_t *s,
                           mulib_cgeom_ray_t ray, unsigned bounces,
                           mulib_linalg_vec3_t *out)
  {
    if (!out || bounces > 8 || !mulib_raytracing_scene_valid (s)
        || !mulib_raytracing_point_ (ray.origin)
        || !mulib_linalg_vec3_normalize (ray.direction, &ray.direction))
      return 0;
    *out = mulib_raytracing_trace_ (s, ray, bounces);
    return 1;
  }
  static float
  mulib_raytracing_srgb_ (float x)
  {
    x = fmaxf (0, fminf (1, x));
    return x <= 0.0031308f ? 12.92f * x
                           : 1.055f * powf (x, 1.0f / 2.4f) - 0.055f;
  }
  int
  mulib_raytracing_render (const mulib_raytracing_scene_t *s,
                           mulib_raytracing_camera_t camera, unsigned bounces,
                           mulib_glx_t *g)
  {
    mulib_linalg_vec3_t forward, right, up;
    float scale, aspect;
    unsigned w = mulib_glx_width (g), h = mulib_glx_height (g);
    if (!g || bounces > 8 || !mulib_raytracing_scene_valid (s)
        || !mulib_raytracing_point_ (camera.eye)
        || !mulib_raytracing_point_ (camera.target) || !isfinite (camera.fovy)
        || camera.fovy <= 0 || camera.fovy >= 3.14159265358979323846
        || !mulib_linalg_vec3_normalize (
            mulib_linalg_vec3_sub (camera.target, camera.eye), &forward)
        || !mulib_linalg_vec3_normalize (camera.up, &up)
        || !mulib_linalg_vec3_normalize (mulib_linalg_vec3_cross (forward, up),
                                         &right))
      return 0;
    up = mulib_linalg_vec3_cross (right, forward);
    scale = (float)tan (camera.fovy / 2.0);
    aspect = (float)w / h;
    for (unsigned y = 0; y < h; ++y)
      for (unsigned x = 0; x < w; ++x)
        {
          float sx = (2 * (x + 0.5f) / w - 1) * scale * aspect,
                sy = (1 - 2 * (y + 0.5f) / h) * scale;
          mulib_cgeom_ray_t ray;
          mulib_linalg_vec3_t color;
          ray.origin = camera.eye;
          ray.direction = mulib_linalg_vec3_add (
              forward,
              mulib_linalg_vec3_add (mulib_linalg_vec3_scale (right, sx),
                                     mulib_linalg_vec3_scale (up, sy)));
          (void)mulib_linalg_vec3_normalize (ray.direction, &ray.direction);
          color = mulib_raytracing_trace_ (s, ray, bounces);
          mulib_glx_pixel (g, (int)x, (int)y,
                           mulib_glx_rgba (mulib_raytracing_srgb_ (color.x),
                                           mulib_raytracing_srgb_ (color.y),
                                           mulib_raytracing_srgb_ (color.z),
                                           1));
        }
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
