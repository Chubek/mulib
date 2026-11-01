#define MULIB_IMPL_RAYTRACING
#define MULIB_IMPL_COLLISION
#define MULIB_IMPL_CGEOM
#define MULIB_IMPL_SIMDMATH
#define MULIB_IMPL_LINALG
#define MULIB_IMPL_GLX
#define MULIB_IMPL_DRM
#define MULIB_IMPL_VECTGFX
#define MULIB_IMPL_VEC
#define MULIB_IMPL_PARSEC
#include "mulib-raytracing.h"
#include "mulib-vectgfx.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

static void
surfaces (void)
{
  uint32_t pixels[24],
      src[4] = { 0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff };
  mulib_glx_t *g;
  mulib_linalg_vec2_t a = { 0, 0 }, b = { 4, 0 }, c = { 0, 4 }, d = { 4, 4 };
  mulib_glx_vertex_t va = { 0, 0, 0.5f, { 1, 0, 0, 1 } },
                     vb = { 4, 0, 0.5f, { 1, 0, 0, 1 } },
                     vc = { 0, 4, 0.5f, { 1, 0, 0, 1 } };
  for (int i = 0; i < 24; ++i)
    pixels[i] = 0x12345678;
  assert (!mulib_glx_wrap (pixels, sizeof pixels, 4, 4, 15));
  assert (!mulib_glx_wrap (pixels, 4, 4, 4, 16));
  g = mulib_glx_wrap (pixels, sizeof pixels, 4, 4, 24);
  assert (g);
  mulib_glx_clear (g, 0);
  mulib_glx_pixel (g, 0, 0, 0x80ff0000);
  assert (pixels[0] == 0x80ff0000);
  mulib_glx_pixel (g, 0, 0, 0x800000ff);
  assert (pixels[0] == 0xc05500aa);
  mulib_glx_clip (g, 1, 1, 2, 2);
  mulib_glx_fill_rect (g, INT_MIN, INT_MIN, INT_MAX, INT_MAX, 0xffffffff);
  assert (pixels[7] == 0);
  mulib_glx_fill_rect (g, -1, -1, 100, 100, 0xffffffff);
  assert (pixels[7] == 0xffffffff && pixels[1] == 0);
  mulib_glx_reset_clip (g);
  mulib_glx_clear (g, 0);
  assert (mulib_glx_fill_triangle (g, a, b, c, 0x80ff0000));
  assert (mulib_glx_fill_triangle (g, b, d, c, 0x80ff0000));
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 4; ++x)
      assert (pixels[y * 6 + x] == 0x80ff0000);
  for (int y = 0; y < 4; ++y)
    assert (pixels[y * 6 + 4] == 0x12345678
            && pixels[y * 6 + 5] == 0x12345678);
  mulib_glx_clear (g, 0);
  assert (mulib_glx_triangle (g, va, vb, vc, NULL, NULL));
  assert (pixels[0] == 0xffff0000);
  va.z = vb.z = vc.z = 0.8f;
  va.color.x = vb.color.x = vc.color.x = 0;
  va.color.z = vb.color.z = vc.color.z = 1;
  assert (mulib_glx_triangle (g, va, vb, vc, NULL, NULL)
          && pixels[0] == 0xffff0000);
  va.z = vb.z = vc.z = 0.2f;
  assert (mulib_glx_triangle (g, va, vb, vc, NULL, NULL)
          && pixels[0] == 0xff0000ff);
  a.x = -1000000;
  a.y = 2;
  b.x = 1000000;
  b.y = 2;
  assert (mulib_glx_line (g, a, b, 0xffffffff));
  for (int x = 0; x < 4; ++x)
    assert (pixels[12 + x] == 0xffffffff);
  assert (mulib_glx_blit (g, 2, 2, src, sizeof src, 2, 2, 8)
          && pixels[14] == 0xffff0000 && pixels[21] == 0xffffffff);
  assert (!mulib_glx_blit (g, 0, 0, src, 1, 2, 2, 8));
  mulib_glx_destroy (g);
  pixels[0] = 42;
  assert (pixels[0] == 42);
  {
    mulib_drm_dumb_t invalid = MULIB_DRM_DUMB_INIT;
    assert (!mulib_glx_map_drm (&invalid));
  }
}
static void
paths (void)
{
  const char *source
      = "M 1 1 L 15 1 L 15 15 L 1 15 Z M 5 5 L 11 5 L 11 11 L 5 11 Z";
  mulib_vectgfx_error_t e;
  mulib_vectgfx_t *p = mulib_vectgfx_parse (source, strlen (source), &e);
  mulib_glx_t *g = mulib_glx_create (16, 16);
  uint32_t *pixels = mulib_glx_pixels (g);
  mulib_linalg_vec2_t a = { 1, 8 }, b = { 15, 8 }, control = { 8, 0 };
  assert (p && g && !e.message);
  assert (mulib_vectgfx_fill (p, g, 0xffffffff, 0.25f));
  assert (pixels[2 * 16 + 2] == 0xffffffff && pixels[8 * 16 + 8] == 0
          && pixels[0] == 0);
  mulib_vectgfx_clear (p);
  mulib_glx_clear (g, 0);
  assert (mulib_vectgfx_move_to (p, a));
  assert (mulib_vectgfx_line_to (p, b));
  assert (mulib_vectgfx_line_to (p, a));
  assert (mulib_vectgfx_stroke (p, g, 0x80ff0000, 4, 0.25f));
  assert (pixels[8 * 16 + 8] == 0x80ff0000);
  assert (!mulib_vectgfx_stroke (p, g, 0xffffffff, -1, 0.25f));
  assert (!mulib_vectgfx_fill (p, g, 0xffffffff, 0));
  mulib_vectgfx_clear (p);
  assert (mulib_vectgfx_move_to (p, a));
  assert (mulib_vectgfx_quad_to (p, control, b));
  mulib_glx_clear (g, 0);
  assert (mulib_vectgfx_stroke (p, g, 0xffffffff, 2, 0.1f));
  assert (pixels[4 * 16 + 8] != 0);
  mulib_vectgfx_clear (p);
  a.x = 8;
  a.y = 8;
  assert (mulib_vectgfx_arc (p, a, 5, 5, 0, -6.2831853f));
  assert (mulib_vectgfx_close (p));
  mulib_glx_clear (g, 0);
  assert (mulib_vectgfx_fill (p, g, 0xff00ff00, 0.1f)
          && pixels[8 * 16 + 8] == 0xff00ff00);
  mulib_vectgfx_destroy (p);
  p = mulib_vectgfx_parse (NULL, 0, &e);
  assert (p && mulib_vectgfx_fill (p, g, 0, 0.1f));
  mulib_vectgfx_destroy (p);
  assert (!mulib_vectgfx_parse ("L 1 2", 5, &e) && e.message);
  assert (!mulib_vectgfx_parse ("M 1", 3, &e) && e.offset == 3);
  assert (!mulib_vectgfx_parse ("M 1e999 2", 9, &e));
  assert (!mulib_vectgfx_parse ("M 0 0 Z L 1 2", 13, &e));
  assert (!mulib_vectgfx_parse ("M 0 0 junk", 10, &e));
  assert (!mulib_vectgfx_parse ("M 1.2.3", 7, &e));
  mulib_glx_destroy (g);
}
static void
rays (void)
{
  mulib_raytracing_object_t objects[2];
  mulib_raytracing_scene_t scene;
  mulib_raytracing_hit_t hit;
  mulib_linalg_vec3_t color;
  mulib_cgeom_ray_t ray = { { 0, 0, 0 }, { 0, 0, -1 } };
  mulib_raytracing_camera_t camera
      = { { 0, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 }, 1.2f };
  mulib_raytracing_light_t light = { { 1, 0, 0 }, { 1, 1, 1 }, 4 };
  mulib_glx_t *g = mulib_glx_create (9, 9);
  memset (objects, 0, sizeof objects);
  memset (&scene, 0, sizeof scene);
  objects[0].kind = MULIB_RAYTRACING_SPHERE;
  objects[0].shape.sphere.center.z = -3;
  objects[0].shape.sphere.radius = 1;
  objects[0].material.emission.x = 1;
  scene.objects = objects;
  scene.object_count = 1;
  scene.background.z = 1;
  assert (mulib_raytracing_intersect (&scene, ray, 0, 100, &hit) == 1
          && hit.object == 0 && hit.geometry.t == 2 && hit.front_face);
  assert (mulib_raytracing_sample (&scene, ray, 0, &color) && color.x == 1
          && color.z == 0);
  assert (mulib_raytracing_render (&scene, camera, 0, g));
  assert (mulib_glx_pixels (g)[40] == 0xffff0000
          && mulib_glx_pixels (g)[0] == 0xff0000ff);
  objects[0].material.emission.x = 0;
  objects[0].material.reflectivity = 1;
  assert (mulib_raytracing_sample (&scene, ray, 1, &color) && color.z == 1
          && color.x == 0);
  objects[0].material.reflectivity = 0;
  objects[0].material.diffuse.x = 1;
  scene.lights = &light;
  scene.light_count = 1;
  assert (mulib_raytracing_sample (&scene, ray, 0, &color) && color.x > 0.5f);
  objects[1] = objects[0];
  objects[1].shape.sphere.center.x = 0.5f;
  objects[1].shape.sphere.center.z = -1;
  objects[1].shape.sphere.radius = 0.2f;
  scene.object_count = 2;
  assert (mulib_raytracing_sample (&scene, ray, 0, &color) && color.x == 0);
  scene.object_count = 0;
  assert (mulib_raytracing_sample (&scene, ray, 8, &color) && color.z == 1);
  scene.object_count = 1;
  objects[0].shape.sphere.radius = -1;
  assert (!mulib_raytracing_render (&scene, camera, 0, g));
  assert (!mulib_raytracing_sample (&scene, ray, 9, &color));
  mulib_glx_destroy (g);
}
int
main (void)
{
  surfaces ();
  paths ();
  rays ();
  return 0;
}
