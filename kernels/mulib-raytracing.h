/**
 * @file mulib-raytracing.h
 * @brief CPU ray renderer with legacy direct shading and seeded path tracing.
 * Define MULIB_IMPL_RAYTRACING plus COLLISION, CGEOM, SIMDMATH, LINALG, GLX and
 * DRM in the implementation translation unit.
 */
#ifndef MULIB_RAYTRACING_H
#define MULIB_RAYTRACING_H
#include <float.h>
#include <math.h>
#include <stdint.h>
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

  enum
  {
    MULIB_RAYTRACING_MAX_SAMPLES = 1024,
    MULIB_RAYTRACING_MAX_BOUNCES = 16,
    MULIB_RAYTRACING_MAX_DIMENSION = 16384
  };
  typedef struct mulib_raytracing_path_options
  {
    uint64_t seed;               /**< All seeds, including zero, are valid. */
    unsigned samples_per_pixel;  /**< 1..MULIB_RAYTRACING_MAX_SAMPLES. */
    unsigned max_bounces;        /**< Secondary rays, 0..MAX_BOUNCES. */
  } mulib_raytracing_path_options_t;
  typedef struct mulib_raytracing_image
  {
    mulib_linalg_vec3_t *pixels; /**< Borrowed writable linear RGB, top down. */
    size_t capacity, stride;     /**< Both in pixels, not bytes. */
    unsigned width, height;
  } mulib_raytracing_image_t;

  /* Shared by the legacy implementation and the path tracer. */
  static inline int
  mulib_raytracing_point_ (mulib_linalg_vec3_t v)
  {
    return mulib_cgeom_vec3_valid (v) && fabsf (v.x) <= 1000000
           && fabsf (v.y) <= 1000000 && fabsf (v.z) <= 1000000;
  }

  static inline int
  mulib_raytracing_primitive_ (const mulib_raytracing_object_t *o,
                               mulib_cgeom_ray_t ray, float lo, float hi,
                               mulib_collision_hit_t *out)
  {
    return o->kind == MULIB_RAYTRACING_SPHERE
               ? mulib_collision_ray_sphere (ray, o->shape.sphere, lo, hi, out)
               : mulib_collision_ray_triangle (ray, o->shape.triangle, lo, hi,
                                               out);
  }

  static inline int
  mulib_raytracing_intersect_ (const mulib_raytracing_scene_t *s,
                               mulib_cgeom_ray_t ray, float lo, float hi,
                               mulib_raytracing_hit_t *out)
  {
    int found = 0;
    mulib_raytracing_hit_t result;
    for (size_t i = 0; i < s->object_count; ++i)
      {
        mulib_collision_hit_t hit;
        int rc
            = mulib_raytracing_primitive_ (&s->objects[i], ray, lo, hi, &hit);
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

  static inline mulib_linalg_vec3_t
  mulib_raytracing_product_ (mulib_linalg_vec3_t a, mulib_linalg_vec3_t b)
  {
    mulib_linalg_vec3_t v = { a.x * b.x, a.y * b.y, a.z * b.z };
    return v;
  }

  static inline float
  mulib_raytracing_offset_ (mulib_linalg_vec3_t p)
  {
    return fmaxf (0.0001f, 8 * FLT_EPSILON
                              * fmaxf (fabsf (p.x),
                                       fmaxf (fabsf (p.y), fabsf (p.z))));
  }

  static inline float
  mulib_raytracing_srgb_ (float x)
  {
    x = fmaxf (0, fminf (1, x));
    return x <= 0.0031308f ? 12.92f * x
                           : 1.055f * powf (x, 1.0f / 2.4f) - 0.055f;
  }

  static inline uint32_t
  mulib_raytracing_argb_ (mulib_linalg_vec3_t color)
  {
    return mulib_glx_rgba (mulib_raytracing_srgb_ (color.x),
                          mulib_raytracing_srgb_ (color.y),
                          mulib_raytracing_srgb_ (color.z), 1);
  }

  typedef struct mulib_raytracing_frame_
  {
    mulib_linalg_vec3_t eye, forward, right, up;
    double half_width, half_height;
    unsigned width, height;
  } mulib_raytracing_frame_;

  static inline int
  mulib_raytracing_frame_init_ (mulib_raytracing_camera_t camera, unsigned w,
                                unsigned h, mulib_raytracing_frame_ *frame)
  {
    mulib_linalg_vec3_t up, cross;
    if (!w || !h || w > MULIB_RAYTRACING_MAX_DIMENSION
        || h > MULIB_RAYTRACING_MAX_DIMENSION
        || !mulib_raytracing_point_ (camera.eye)
        || !mulib_raytracing_point_ (camera.target) || !isfinite (camera.fovy)
        || camera.fovy <= 0 || camera.fovy >= 3.14159265358979323846
        || !mulib_linalg_vec3_normalize (
            mulib_linalg_vec3_sub (camera.target, camera.eye), &frame->forward)
        || !mulib_linalg_vec3_normalize (camera.up, &up))
      return 0;
    cross = mulib_linalg_vec3_cross (frame->forward, up);
    if (mulib_linalg_vec3_length (cross) <= 1e-6f
        || !mulib_linalg_vec3_normalize (cross, &frame->right))
      return 0;
    frame->up = mulib_linalg_vec3_cross (frame->right, frame->forward);
    frame->eye = camera.eye;
    frame->half_height = tan (camera.fovy / 2.0);
    frame->half_width = frame->half_height * w / h;
    frame->width = w;
    frame->height = h;
    return 1;
  }

  static inline mulib_cgeom_ray_t
  mulib_raytracing_frame_ray_ (const mulib_raytracing_frame_ *frame, unsigned x,
                               unsigned y, double u, double v)
  {
    float sx = (float)((2 * (x + u) / frame->width - 1) * frame->half_width);
    float sy = (float)((1 - 2 * (y + v) / frame->height) * frame->half_height);
    mulib_cgeom_ray_t ray;
    ray.origin = frame->eye;
    ray.direction = mulib_linalg_vec3_add (
        frame->forward, mulib_linalg_vec3_add (
                            mulib_linalg_vec3_scale (frame->right, sx),
                            mulib_linalg_vec3_scale (frame->up, sy)));
    (void)mulib_linalg_vec3_normalize (ray.direction, &ray.direction);
    return ray;
  }

  /** @brief Make a normalized pinhole ray. (x,y) names a top-left-origin pixel;
   * (u,v) are finite offsets in [0,1), with (0.5,0.5) the center. Image
   * dimensions are 1..MAX_DIMENSION and fovy is in radians. Returns 1, or 0
   * on invalid input without modifying out.
   */
  static inline int
  mulib_raytracing_camera_ray (mulib_raytracing_camera_t camera, unsigned w,
                               unsigned h, unsigned x, unsigned y, double u,
                               double v, mulib_cgeom_ray_t *out)
  {
    mulib_raytracing_frame_ frame;
    if (!out || x >= w || y >= h || !(u >= 0 && u < 1 && v >= 0 && v < 1)
        || !mulib_raytracing_frame_init_ (camera, w, h, &frame))
      return 0;
    *out = mulib_raytracing_frame_ray_ (&frame, x, y, u, v);
    return 1;
  }

  static inline int
  mulib_raytracing_options_valid_ (mulib_raytracing_path_options_t options)
  {
    return options.samples_per_pixel > 0
           && options.samples_per_pixel <= MULIB_RAYTRACING_MAX_SAMPLES
           && options.max_bounces <= MULIB_RAYTRACING_MAX_BOUNCES;
  }

  /* SplitMix64 finalizer and stream. A pixel/sample starts its own stream, so
     path length and render order cannot change the next sample's sequence. */
  static inline uint64_t
  mulib_raytracing_mix_ (uint64_t n)
  {
    n = (n ^ (n >> 30)) * UINT64_C (0xbf58476d1ce4e5b9);
    n = (n ^ (n >> 27)) * UINT64_C (0x94d049bb133111eb);
    return n ^ (n >> 31);
  }

  static inline double
  mulib_raytracing_random_ (uint64_t *state)
  {
    *state += UINT64_C (0x9e3779b97f4a7c15);
    return (double)(mulib_raytracing_mix_ (*state) >> 11)
           * (1.0 / 9007199254740992.0);
  }

  static inline mulib_linalg_vec3_t
  mulib_raytracing_cosine_ (mulib_linalg_vec3_t n, uint64_t *state)
  {
    const mulib_linalg_vec3_t z = { 0, 0, 1 }, x = { 1, 0, 0 };
    mulib_linalg_vec3_t t = mulib_linalg_vec3_cross (fabsf (n.z) < 0.9f ? z : x,
                                                    n);
    mulib_linalg_vec3_t b, result;
    double u = mulib_raytracing_random_ (state);
    double phi = 6.28318530717958647692 * mulib_raytracing_random_ (state);
    double r = sqrt (u);
    (void)mulib_linalg_vec3_normalize (t, &t);
    b = mulib_linalg_vec3_cross (n, t);
    result = mulib_linalg_vec3_add (
        mulib_linalg_vec3_scale (n, (float)sqrt (1 - u)),
        mulib_linalg_vec3_add (
            mulib_linalg_vec3_scale (t, (float)(r * cos (phi))),
            mulib_linalg_vec3_scale (b, (float)(r * sin (phi)))));
    return result;
  }

  static inline int
  mulib_raytracing_occluded_ (const mulib_raytracing_scene_t *s,
                              mulib_cgeom_ray_t ray, float end)
  {
    for (size_t i = 0; i < s->object_count; ++i)
      if (mulib_raytracing_primitive_ (&s->objects[i], ray, 0, end, NULL) != 0)
        return 1;
    return 0;
  }

  static inline mulib_linalg_vec3_t
  mulib_raytracing_direct_ (const mulib_raytracing_scene_t *s,
                             mulib_linalg_vec3_t p, mulib_linalg_vec3_t origin,
                             mulib_linalg_vec3_t n, float epsilon,
                             const mulib_raytracing_material_t *m)
  {
    mulib_linalg_vec3_t direct = { 0, 0, 0 };
    if (m->reflectivity == 1
        || (m->diffuse.x == 0 && m->diffuse.y == 0 && m->diffuse.z == 0))
      return direct;
    for (size_t i = 0; i < s->light_count; ++i)
      {
        const mulib_raytracing_light_t *light = &s->lights[i];
        mulib_linalg_vec3_t to = mulib_linalg_vec3_sub (light->position, p);
        double d2 = (double)to.x * to.x + (double)to.y * to.y
                    + (double)to.z * to.z;
        float cosine;
        mulib_cgeom_ray_t shadow;
        if (light->intensity == 0 || d2 <= 4.0 * epsilon * epsilon)
          continue;
        cosine = (float)(mulib_linalg_vec3_dot (n, to) / sqrt (d2));
        if (cosine <= 0)
          continue;
        shadow.origin = origin;
        shadow.direction = mulib_linalg_vec3_sub (light->position, origin);
        /* The unnormalized direction puts the light at t=1. */
        if (!mulib_raytracing_occluded_ (s, shadow, nextafterf (1, 0)))
          {
            mulib_linalg_vec3_t tint
                = mulib_raytracing_product_ (m->diffuse, light->color);
            float weight = (float)((1 - m->reflectivity) * light->intensity
                                   * fminf (cosine, 1)
                                   / (3.14159265358979323846 * d2));
            direct = mulib_linalg_vec3_add (
                direct, mulib_linalg_vec3_scale (tint, weight));
          }
      }
    return direct;
  }

  static inline mulib_linalg_vec3_t
  mulib_raytracing_path_ (const mulib_raytracing_scene_t *s,
                          mulib_cgeom_ray_t ray, unsigned max_bounces,
                          uint64_t *state)
  {
    mulib_linalg_vec3_t sum = { 0, 0, 0 }, beta = { 1, 1, 1 };
    for (unsigned bounce = 0;; ++bounce)
      {
        mulib_raytracing_hit_t hit;
        mulib_linalg_vec3_t n, origin, direct;
        const mulib_raytracing_material_t *m;
        float epsilon;
        if (mulib_raytracing_intersect_ (s, ray, 0.0001f, FLT_MAX, &hit) != 1)
          return mulib_linalg_vec3_add (
              sum, mulib_raytracing_product_ (beta, s->background));
        m = &s->objects[hit.object].material;
        n = hit.geometry.normal;
        if (!hit.front_face)
          n = mulib_linalg_vec3_scale (n, -1);
        epsilon = mulib_raytracing_offset_ (hit.geometry.point);
        origin = mulib_linalg_vec3_add (hit.geometry.point,
                                        mulib_linalg_vec3_scale (n, epsilon));
        direct = mulib_raytracing_direct_ (s, hit.geometry.point, origin, n,
                                           epsilon, m);
        sum = mulib_linalg_vec3_add (
            sum, mulib_raytracing_product_ (
                     beta, mulib_linalg_vec3_add (m->emission, direct)));
        if (bounce == max_bounces)
          return sum;

        /* The lobe selection PDF cancels the lobe weight. The diffuse
           direction PDF also cancels the Lambert cosine and 1/pi. */
        if (mulib_raytracing_random_ (state) < m->reflectivity)
          {
            float d = mulib_linalg_vec3_dot (ray.direction, n);
            ray.direction = mulib_linalg_vec3_sub (
                ray.direction, mulib_linalg_vec3_scale (n, 2 * d));
          }
        else
          {
            beta = mulib_raytracing_product_ (beta, m->diffuse);
            if (beta.x == 0 && beta.y == 0 && beta.z == 0)
              return sum;
            ray.direction = mulib_raytracing_cosine_ (n, state);
          }
        (void)mulib_linalg_vec3_normalize (ray.direction, &ray.direction);
        ray.origin = origin;
      }
  }

  static inline mulib_linalg_vec3_t
  mulib_raytracing_average_ (const mulib_raytracing_scene_t *s,
                              mulib_raytracing_path_options_t options,
                              const mulib_raytracing_frame_ *frame, unsigned x,
                              unsigned y, const mulib_cgeom_ray_t *fixed_ray)
  {
    double r = 0, g = 0, b = 0;
    uint64_t key = mulib_raytracing_mix_ (options.seed)
                   ^ ((uint64_t)y << 32 | x);
    for (unsigned i = 0; i < options.samples_per_pixel; ++i)
      {
        uint64_t state = mulib_raytracing_mix_ (
            key + (uint64_t)i * UINT64_C (0xd2b74407b1ce6e93));
        mulib_cgeom_ray_t ray;
        mulib_linalg_vec3_t sample;
        if (frame)
          {
            double u = mulib_raytracing_random_ (&state);
            double v = mulib_raytracing_random_ (&state);
            ray = mulib_raytracing_frame_ray_ (frame, x, y, u, v);
          }
        else
          ray = *fixed_ray;
        sample = mulib_raytracing_path_ (s, ray, options.max_bounces, &state);
        r += sample.x;
        g += sample.y;
        b += sample.z;
      }
    {
      mulib_linalg_vec3_t result = { (float)(r / options.samples_per_pixel),
                                     (float)(g / options.samples_per_pixel),
                                     (float)(b / options.samples_per_pixel) };
      return result;
    }
  }

  /** @brief Average seeded paths along one ray (no subpixel jitter). Returns
   * unclamped linear RGB; direction is normalized internally. Path APIs use
   * emission, an environment, explicit point lights and diffuse/mirror
   * transport; scene.ambient is validated but not used. Returns 1, or 0 on
   * invalid input without touching out.
   */
  static inline int
  mulib_raytracing_path_sample (const mulib_raytracing_scene_t *s,
                                mulib_cgeom_ray_t ray,
                                mulib_raytracing_path_options_t options,
                                mulib_linalg_vec3_t *out)
  {
    if (!out || !mulib_raytracing_options_valid_ (options)
        || !mulib_raytracing_scene_valid (s)
        || !mulib_raytracing_point_ (ray.origin)
        || !mulib_linalg_vec3_normalize (ray.direction, &ray.direction))
      return 0;
    *out = mulib_raytracing_average_ (s, options, NULL, 0, 0, &ray);
    return 1;
  }

  /** @brief Average seeded, uniformly jittered paths within a top-down pixel.
   * Output is unclamped linear RGB. Same samples as either full-frame API.
   * Returns 1, or 0 on invalid input without touching out.
   */
  static inline int
  mulib_raytracing_path_pixel (const mulib_raytracing_scene_t *s,
                               mulib_raytracing_camera_t camera, unsigned w,
                               unsigned h, unsigned x, unsigned y,
                               mulib_raytracing_path_options_t options,
                               mulib_linalg_vec3_t *out)
  {
    mulib_raytracing_frame_ frame;
    if (!out || x >= w || y >= h || !mulib_raytracing_options_valid_ (options)
        || !mulib_raytracing_frame_init_ (camera, w, h, &frame)
        || !mulib_raytracing_scene_valid (s))
      return 0;
    *out = mulib_raytracing_average_ (s, options, &frame, x, y, NULL);
    return 1;
  }

  /** @brief Render to caller-owned, strided linear RGB. The image must not
   * alias any scene input. Write only visible-width pixels (not padding).
   * Returns 1, or 0 on invalid input without writing any pixels.
   */
  static inline int
  mulib_raytracing_path_render (const mulib_raytracing_scene_t *s,
                                mulib_raytracing_camera_t camera,
                                mulib_raytracing_path_options_t options,
                                mulib_raytracing_image_t image)
  {
    mulib_raytracing_frame_ frame;
    if (!image.pixels || !mulib_raytracing_options_valid_ (options)
        || !mulib_raytracing_frame_init_ (camera, image.width, image.height,
                                          &frame)
        || image.stride < image.width || image.capacity < image.width
        || image.capacity > SIZE_MAX / sizeof (*image.pixels)
        || image.height - 1 > (image.capacity - image.width) / image.stride
        || !mulib_raytracing_scene_valid (s))
      return 0;
    for (unsigned y = 0; y < image.height; ++y)
      for (unsigned x = 0; x < image.width; ++x)
        image.pixels[y * image.stride + x]
            = mulib_raytracing_average_ (s, options, &frame, x, y, NULL);
    return 1;
  }

  /** @brief Render the same pixel averages as path_render to an opaque sRGB
   * GLX surface (clamp, no tone mapping). Respects the scissor on writes;
   * does not change depth. Returns 1, or 0 before writing on invalid input.
   */
  static inline int
  mulib_raytracing_path_render_glx (const mulib_raytracing_scene_t *s,
                                    mulib_raytracing_camera_t camera,
                                    mulib_raytracing_path_options_t options,
                                    mulib_glx_t *surface)
  {
    mulib_raytracing_frame_ frame;
    unsigned w = mulib_glx_width (surface), h = mulib_glx_height (surface);
    if (!mulib_raytracing_options_valid_ (options)
        || !mulib_raytracing_frame_init_ (camera, w, h, &frame)
        || !mulib_raytracing_scene_valid (s))
      return 0;
    for (unsigned y = 0; y < h; ++y)
      for (unsigned x = 0; x < w; ++x)
        {
          mulib_linalg_vec3_t c
              = mulib_raytracing_average_ (s, options, &frame, x, y, NULL);
          mulib_glx_pixel (surface, (int)x, (int)y, mulib_raytracing_argb_ (c));
        }
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_RAYTRACING) && !defined(MULIB_RAYTRACING_IMPLEMENTED)
#define MULIB_RAYTRACING_IMPLEMENTED
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
    epsilon = mulib_raytracing_offset_ (hit.geometry.point);
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
          mulib_glx_pixel (g, (int)x, (int)y, mulib_raytracing_argb_ (color));
        }
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
