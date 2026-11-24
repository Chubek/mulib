/** @file mulib-render-shadow.h
 * Ray-tested visibility to a point on a light.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No shadow maps; requires validated scene.
 */
#ifndef MULIB_RENDER_SHADOW_H
#define MULIB_RENDER_SHADOW_H
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
static inline bool mulib_render_shadow_visible(mulib_render_scene s,mulib_mesh_vec3 p,mulib_mesh_vec3 n,mulib_mesh_vec3 direction,double distance){if(!isfinite(distance)||distance<=2e-6)return false;mulib_render_ray r={mulib_mesh_add(p,mulib_mesh_scale(n,1e-6)),direction};mulib_render_hit h;return !mulib_render_scene_hit(s,r,1e-6,distance-1e-6,&h);}
#ifdef __cplusplus
}
#endif
#endif
