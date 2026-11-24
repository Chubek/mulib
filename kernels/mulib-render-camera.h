/** @file mulib-render-camera.h
 * Pinhole look-at camera with normalized pixel rays.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_RENDER_CAMERA_H
#define MULIB_RENDER_CAMERA_H
#include "mulib-mesh-core.h"
#include "mulib-render-scene.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {mulib_mesh_vec3 eye,forward,right,up;double tan_half_fov,aspect;} mulib_render_camera;
static inline bool mulib_render_camera_init(mulib_render_camera *c,mulib_mesh_vec3 eye,mulib_mesh_vec3 target,mulib_mesh_vec3 up,double fov,double aspect){if(!c||!mulib_mesh_finite(eye)||!mulib_mesh_finite(target)||!mulib_mesh_finite(up)||!isfinite(fov)||fov<=0||fov>=3.141592653589793||!isfinite(aspect)||aspect<=0)return false;mulib_mesh_vec3 f=mulib_mesh_unit(mulib_mesh_sub(target,eye)),r=mulib_mesh_unit(mulib_mesh_cross(f,up));if(!mulib_mesh_length(f)||!mulib_mesh_length(r))return false;mulib_render_camera z={eye,f,r,mulib_mesh_cross(r,f),tan(fov*.5),aspect};*c=z;return true;}
static inline mulib_render_ray mulib_render_camera_ray(mulib_render_camera c,double u,double v){mulib_render_ray r={c.eye,mulib_mesh_unit(mulib_mesh_add(c.forward,mulib_mesh_add(mulib_mesh_scale(c.right,(2*u-1)*c.aspect*c.tan_half_fov),mulib_mesh_scale(c.up,(1-2*v)*c.tan_half_fov))))};return r;}
#ifdef __cplusplus
}
#endif
#endif
