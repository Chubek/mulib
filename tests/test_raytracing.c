#include "mulib-raytracing.h"
#include "mulib-raytracing.h"
#include <assert.h>
#include <limits.h>

static const mulib_raytracing_camera_t camera
    = { { 0, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 }, 1.0f };
static const mulib_cgeom_ray_t primary = { { 0, 0, 0 }, { 0, 0, -4 } };
static const mulib_linalg_vec3_t canary = { -1, -2, -3 };

static int
same (mulib_linalg_vec3_t a, mulib_linalg_vec3_t b)
{
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

static mulib_raytracing_object_t
plane (float z)
{
  mulib_raytracing_object_t o = { 0 };
  o.kind = MULIB_RAYTRACING_TRIANGLE;
  o.shape.triangle = (mulib_cgeom_triangle_t){ { -16, -16, z },
                                             { 16, -16, z },
                                             { 0, 16, z } };
  return o;
}

static mulib_raytracing_object_t
sphere (mulib_linalg_vec3_t center, float radius)
{
  mulib_raytracing_object_t o = { 0 };
  o.kind = MULIB_RAYTRACING_SPHERE;
  o.shape.sphere = (mulib_cgeom_sphere_t){ center, radius };
  return o;
}

static void
camera_and_geometry (void)
{
  mulib_cgeom_ray_t r;
  mulib_raytracing_hit_t hit;
  mulib_raytracing_object_t objects[2] = { plane (-3), plane (-2) };
  mulib_raytracing_scene_t scene = { .objects = objects, .object_count = 2 };
  assert (mulib_raytracing_camera_ray (camera, 3, 3, 1, 1, 0.5, 0.5, &r));
  assert (same (r.origin, camera.eye) && same (r.direction, camera.target));
  assert (mulib_raytracing_intersect (&scene, r, 0, 2, &hit) == 1);
  assert (hit.object == 1 && hit.front_face && hit.geometry.t == 2);
  assert (same (hit.geometry.barycentric,
                (mulib_linalg_vec3_t){ .25f, .25f, .5f }));
  assert (mulib_raytracing_intersect (&scene, primary, .5f, .5f, &hit) == 1);
  assert (hit.geometry.t == .5f);
  assert (mulib_raytracing_camera_ray (camera, 4, 2, 0, 0, 0.5, 0.5, &r));
  assert (r.direction.x < 0 && r.direction.y > 0 && r.direction.z < 0);
  assert (fabsf (r.direction.x / r.direction.y + 3) < 1e-6f);
  assert (fabsf (mulib_linalg_vec3_length (r.direction) - 1) < 1e-6f);
  scene.object_count = 1;
  objects[0] = sphere ((mulib_linalg_vec3_t){ 0, 0, 0 }, 1);
  assert (mulib_raytracing_intersect (&scene, primary, 0, 1, &hit) == 1);
  assert (!hit.front_face && hit.geometry.t == .25f);
}

static unsigned
srgb8 (float x)
{
  double encoded = x <= 0.0031308 ? 12.92 * x : 1.055 * pow (x, 1 / 2.4) - .055;
  return (unsigned)lround (255 * encoded);
}

static void
antialias_and_determinism (void)
{
  enum { W = 9, H = 7, STRIDE = 12 };
  mulib_linalg_vec3_t a[STRIDE * H], b[STRIDE * H];
  uint32_t glx_pixels[STRIDE * H];
  const uint32_t sentinel = UINT32_C (0x12345678);
  mulib_raytracing_image_t image = { a, (H - 1) * STRIDE + W, STRIDE, W, H };
  mulib_raytracing_path_options_t options = { 42, 256, 0 };
  mulib_raytracing_object_t edge = plane (-2);
  mulib_raytracing_scene_t scene = { .objects = &edge, .object_count = 1,
                                     .background = { 0, 0, 1 } };
  mulib_glx_t *glx = mulib_glx_wrap (glx_pixels, sizeof glx_pixels, W, H,
                                    STRIDE * sizeof (*glx_pixels));
  mulib_glx_vertex_t v0 = { 4, 2, .25f, { 0, 1, 0, 1 } },
                     v1 = { 6, 2, .25f, { 0, 1, 0, 1 } },
                     v2 = { 4, 4, .25f, { 0, 1, 0, 1 } };
  uint32_t color_after_render;
  unsigned changed = 0;
  edge.shape.triangle.a.x = 0;
  edge.shape.triangle.b.y = 0;
  edge.material.emission.x = 1;
  assert (glx);
  for (unsigned i = 0; i < STRIDE * H; ++i)
    {
      a[i] = b[i] = canary;
      glx_pixels[i] = sentinel;
    }
  assert (mulib_raytracing_path_render (&scene, camera, options, image));
  image.pixels = b;
  assert (mulib_raytracing_path_render (&scene, camera, options, image));
  for (unsigned i = 0; i < STRIDE * H; ++i)
    assert (same (a[i], b[i]));
  mulib_glx_clip (glx, 2, 1, 5, 3);
  assert (mulib_glx_triangle (glx, v0, v1, v2, NULL, NULL));
  assert (glx_pixels[2 * STRIDE + 4] == UINT32_C (0xff00ff00));
  assert (mulib_raytracing_path_render_glx (&scene, camera, options, glx));
  color_after_render = glx_pixels[2 * STRIDE + 4];
  v0.z = v1.z = v2.z = .75f;
  assert (mulib_glx_triangle (glx, v0, v1, v2, NULL, NULL));
  assert (glx_pixels[2 * STRIDE + 4] == color_after_render);
  for (unsigned j = H; j > 0; --j)
    for (unsigned i = STRIDE; i > 0; --i)
      {
        unsigned x = i - 1, y = j - 1, index = y * STRIDE + x;
        mulib_linalg_vec3_t expected = a[index], pixel;
        if (x < W)
          {
            assert (mulib_raytracing_path_pixel (&scene, camera, W, H, x, y,
                                                 options, &pixel));
            assert (same (pixel, expected));
            assert (pixel.x + pixel.z == 1 && pixel.y == 0);
            if (x == W / 2)
              assert (pixel.x > .35f && pixel.x < .65f);
            else
              assert (pixel.x == (x > W / 2));
          }
        else
          assert (same (expected, canary));
        if (x >= 2 && x < 7 && y >= 1 && y < 4)
          assert (glx_pixels[index] == (UINT32_C (0xff000000)
                                         | (srgb8 (expected.x) << 16)
                                         | srgb8 (expected.z)));
        else
          assert (glx_pixels[index] == sentinel);
      }
  options.seed = 43;
  assert (mulib_raytracing_path_render (&scene, camera, options, image));
  for (unsigned y = 0; y < H; ++y)
    changed += !same (a[y * STRIDE + W / 2], b[y * STRIDE + W / 2]);
  assert (changed > 0);
  options.seed = 0;
  options.samples_per_pixel = 1;
  assert (mulib_raytracing_path_pixel (&scene, camera, 1, 1, 0, 0, options, b));
  assert (b[0].x == 0 || b[0].x == 1);
  mulib_glx_destroy (glx);
}

static void
lighting_and_bounces (void)
{
  mulib_raytracing_object_t objects[2] = { plane (-2), plane (2) };
  mulib_raytracing_scene_t scene = { .objects = objects, .object_count = 1,
                                     .ambient = { 10, 10, 10 },
                                     .background = { 1, .5f, .25f } };
  mulib_raytracing_light_t light = { { 2, 0, 0 }, { 1, .5f, .25f },
                                     35.5430635053f }; /* 8 pi sqrt(2) */
  mulib_raytracing_path_options_t options = { 81, 1, 0 };
  mulib_linalg_vec3_t c, repeated, black = { 0, 0, 0 };
  objects[0].material.diffuse = (mulib_linalg_vec3_t){ .5f, .25f, 1 };
  assert (mulib_raytracing_path_sample (&scene, primary, options, &c));
  assert (same (c, black)); /* No legacy ambient term. */
  options.max_bounces = 1;
  assert (mulib_raytracing_path_sample (&scene, primary, options, &c));
  assert (same (c, (mulib_linalg_vec3_t){ .5f, .125f, .25f }));
  objects[0].material.reflectivity = 1;
  assert (mulib_raytracing_path_sample (&scene, primary, options, &c));
  assert (same (c, scene.background));
  objects[0].material.reflectivity = .25f;
  options.samples_per_pixel = MULIB_RAYTRACING_MAX_SAMPLES;
  assert (mulib_raytracing_path_sample (&scene, primary, options, &c));
  assert (mulib_raytracing_path_sample (&scene, primary, options, &repeated));
  assert (same (c, repeated));
  assert (fabsf (c.x - .625f) < .035f && fabsf (c.y - .21875f) < .025f
          && c.z == .25f);

  scene.background = black;
  scene.lights = &light;
  scene.light_count = 1;
  options.max_bounces = 0;
  options.samples_per_pixel = 2;
  assert (mulib_raytracing_path_sample (&scene, primary, options, &c));
  assert (fabsf (c.x - .375f) < 1e-6f && fabsf (c.y - .09375f) < 1e-6f
          && fabsf (c.z - .1875f) < 1e-6f);
  scene.object_count = 2;
  objects[1] = sphere ((mulib_linalg_vec3_t){ 3, 0, 1 }, .25f);
  assert (mulib_raytracing_path_sample (&scene, primary, options, &repeated));
  assert (same (c, repeated)); /* The shadow ray stops at the point light. */
  objects[1].shape.sphere.center = (mulib_linalg_vec3_t){ 1, 0, -1 };
  assert (mulib_raytracing_path_sample (&scene, primary, options, &c));
  assert (same (c, black));
  light.position = (mulib_linalg_vec3_t){ 0, 0, -1.99995f };
  light.intensity = 1000000;
  assert (mulib_raytracing_path_sample (&scene, primary, options, &c));
  assert (same (c, black)); /* Skip singular/nearby point lights. */

  /* Diffuse paths can carry emission from geometry not visible to the eye. */
  scene.light_count = 0;
  objects[0].material.reflectivity = 0;
  objects[1] = sphere ((mulib_linalg_vec3_t){ 0, 0, 2.5f }, 2);
  objects[1].material.emission = (mulib_linalg_vec3_t){ .8f, .4f, .2f };
  assert (mulib_raytracing_path_sample (&scene, primary, options, &c));
  assert (same (c, black));
  options.max_bounces = 1;
  options.samples_per_pixel = MULIB_RAYTRACING_MAX_SAMPLES;
  assert (mulib_raytracing_path_sample (&scene, primary, options, &c));
  /* Cosine-weighted hit probability on axis is radius^2 / distance^2. */
  assert (fabsf (c.x - .4f * 4 / (4.5f * 4.5f)) < .025f);
  assert (c.y == c.x * .25f && c.z == c.x * .5f);

  /* Two mirrors face the path; each visited surface counts exactly once. */
  objects[1] = plane (2);
  objects[0].material.reflectivity = objects[1].material.reflectivity = 1;
  objects[0].material.emission = (mulib_linalg_vec3_t){ .25f, 0, .5f };
  objects[1].material.emission = (mulib_linalg_vec3_t){ 0, 1, 0 };
  options.samples_per_pixel = 1;
  options.max_bounces = MULIB_RAYTRACING_MAX_BOUNCES;
  assert (mulib_raytracing_path_sample (&scene, primary, options, &c));
  assert (same (c, (mulib_linalg_vec3_t){ 2.25f, 8, 4.5f }));
}

static void
invalid_inputs (void)
{
  mulib_raytracing_camera_t bad_camera = camera;
  mulib_raytracing_object_t bad_object = sphere ((mulib_linalg_vec3_t){ 0 }, 0);
  mulib_raytracing_scene_t scene = { 0 };
  mulib_raytracing_path_options_t options = { 0, 1, 0 }, bad_options = options;
  mulib_linalg_vec3_t pixels[6], result = canary;
  mulib_raytracing_image_t image = { pixels, 6, 3, 2, 2 }, bad_image = image;
  mulib_cgeom_ray_t ray = primary, bad_ray = primary;
  uint32_t argb = UINT32_C (0x12345678);
  mulib_glx_t *glx = mulib_glx_wrap (&argb, sizeof argb, 1, 1, sizeof argb);
  assert (glx);
  for (unsigned i = 0; i < 6; ++i)
    pixels[i] = canary;
  bad_camera.up = camera.target;
  assert (!mulib_raytracing_camera_ray (bad_camera, 1, 1, 0, 0, .5, .5, &ray));
  bad_camera.up.x = 1e-7f;
  assert (!mulib_raytracing_camera_ray (bad_camera, 1, 1, 0, 0, .5, .5, &ray));
  bad_camera = camera;
  bad_camera.target = bad_camera.eye;
  assert (!mulib_raytracing_camera_ray (bad_camera, 1, 1, 0, 0, .5, .5, &ray));
  assert (!mulib_raytracing_camera_ray (camera, 1, 1, 0, 0, NAN, .5, &ray));
  assert (!mulib_raytracing_camera_ray (camera, 1, 1, 0, 0, .5, 1, &ray));
  assert (!mulib_raytracing_camera_ray (camera, 1, 1, 0, 0, -.1, .5, &ray));
  assert (same (ray.origin, primary.origin)
          && same (ray.direction, primary.direction));
  bad_camera = camera;
  bad_camera.fovy = INFINITY;
  assert (!mulib_raytracing_path_render (&scene, bad_camera, options, image));
  assert (!mulib_raytracing_path_render_glx (&scene, bad_camera, options, glx));
  assert (!mulib_raytracing_path_render_glx (&scene, camera, options, NULL));
  assert (!mulib_raytracing_path_pixel (&scene, camera, 2, 2, 0, 2, options,
                                       &result));
  assert (!mulib_raytracing_path_pixel (&scene, camera, 0, 2, 0, 0, options,
                                       &result));
  assert (!mulib_raytracing_path_pixel (&scene, camera, UINT_MAX, 2, 0, 0,
                                       options, &result));
  bad_options.samples_per_pixel = 0;
  assert (!mulib_raytracing_path_sample (&scene, ray, bad_options, &result));
  bad_options.samples_per_pixel = MULIB_RAYTRACING_MAX_SAMPLES + 1;
  assert (!mulib_raytracing_path_sample (&scene, ray, bad_options, &result));
  bad_options = options;
  bad_options.max_bounces = MULIB_RAYTRACING_MAX_BOUNCES + 1;
  assert (!mulib_raytracing_path_render (&scene, camera, bad_options, image));
  bad_ray.direction.z = 0;
  assert (!mulib_raytracing_path_sample (&scene, bad_ray, options, &result));
  bad_ray = ray;
  bad_ray.origin.x = 1000001;
  assert (!mulib_raytracing_path_sample (&scene, bad_ray, options, &result));
  assert (!mulib_raytracing_path_sample (&scene, ray, options, NULL));
  bad_image.pixels = NULL;
  assert (!mulib_raytracing_path_render (&scene, camera, options, bad_image));
  bad_image = image;
  bad_image.capacity = 4; /* The second row needs one more element. */
  assert (!mulib_raytracing_path_render (&scene, camera, options, bad_image));
  bad_image.capacity = SIZE_MAX;
  assert (!mulib_raytracing_path_render (&scene, camera, options, bad_image));
  bad_image = image;
  bad_image.stride = 1;
  assert (!mulib_raytracing_path_render (&scene, camera, options, bad_image));
  bad_image.stride = SIZE_MAX;
  assert (!mulib_raytracing_path_render (&scene, camera, options, bad_image));
  scene.objects = &bad_object;
  scene.object_count = 1;
  assert (!mulib_raytracing_path_render (&scene, camera, options, image));
  assert (!mulib_raytracing_path_render_glx (&scene, camera, options, glx));
  scene.object_count = 65537; /* Rejected without dereferencing the array. */
  assert (!mulib_raytracing_path_render (&scene, camera, options, image));
  assert (!mulib_raytracing_path_render (NULL, camera, options, image));
  scene.object_count = 0;
  scene.light_count = 1; /* No light array. */
  assert (!mulib_raytracing_path_sample (&scene, ray, options, &result));
  scene.light_count = 0;
  scene.background.x = NAN;
  assert (!mulib_raytracing_path_sample (&scene, ray, options, &result));
  for (unsigned i = 0; i < 6; ++i)
    assert (same (pixels[i], canary));
  assert (same (result, canary) && argb == UINT32_C (0x12345678));
  mulib_glx_destroy (glx);
}

int
main (void)
{
  camera_and_geometry ();
  antialias_and_determinism ();
  lighting_and_bounces ();
  invalid_inputs ();
  return 0;
}
