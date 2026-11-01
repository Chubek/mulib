#define MULIB_IMPL_LINALG
#define MULIB_IMPL_SIMDMATH
#define MULIB_IMPL_CGEOM
#define MULIB_IMPL_COLLISION
#define MULIB_IMPL_DRM
#define MULIB_IMPL_GLX
#define MULIB_IMPL_RAYTRACING
#include "mulib-raytracing.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int
main (void)
{
  mulib_raytracing_object_t object;
  mulib_raytracing_scene_t scene;
  mulib_raytracing_light_t light = { { -3, 4, 0 }, { 1, 1, 1 }, 30 };
  mulib_raytracing_camera_t camera
      = { { 0, 0, 0 }, { 0, 0, -3 }, { 0, 1, 0 }, 1.0f };
  mulib_glx_t *surface = mulib_glx_create (96, 64);
  FILE *image;
  memset (&object, 0, sizeof object);
  memset (&scene, 0, sizeof scene);
  object.kind = MULIB_RAYTRACING_SPHERE;
  object.shape.sphere.center.z = -3;
  object.shape.sphere.radius = 1;
  object.material.diffuse.x = 0.8f;
  object.material.diffuse.y = 0.2f;
  object.material.diffuse.z = 0.1f;
  object.material.reflectivity = 0.15f;
  scene.objects = &object;
  scene.object_count = 1;
  scene.lights = &light;
  scene.light_count = 1;
  scene.background.z = 0.1f;
  scene.ambient.x = scene.ambient.y = scene.ambient.z = 0.05f;
  assert (surface && mulib_raytracing_render (&scene, camera, 2, surface));
  image = fopen ("raytracing.ppm", "wb");
  assert (image);
  assert (fprintf (image, "P6\n96 64\n255\n") > 0);
  for (unsigned y = 0; y < 64; ++y)
    for (unsigned x = 0; x < 96; ++x)
      {
        uint32_t color = mulib_glx_pixels (
            surface)[y * (mulib_glx_pitch (surface) / 4) + x];
        unsigned char rgb[3]
            = { (unsigned char)(color >> 16), (unsigned char)(color >> 8),
                (unsigned char)color };
        assert (fwrite (rgb, 1, 3, image) == 3);
      }
  assert (!fclose (image));
  mulib_glx_destroy (surface);
  return 0;
}
