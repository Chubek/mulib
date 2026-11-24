/** @file mulib-render-light.h
 * Inverse-square point-light evaluation.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No area-light sampling.
 */
#ifndef MULIB_RENDER_LIGHT_H
#define MULIB_RENDER_LIGHT_H
#include "mulib-mesh-core.h"
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
typedef struct {mulib_mesh_vec3 position,intensity;} mulib_render_light;
static inline bool mulib_render_light_sample(mulib_render_light l,mulib_mesh_vec3 p,mulib_mesh_vec3 *direction,mulib_mesh_vec3 *radiance,double *distance){if(!direction||!radiance||!distance||!mulib_mesh_finite(l.position)||!mulib_mesh_finite(l.intensity)||!mulib_mesh_finite(p))return false;mulib_mesh_vec3 d=mulib_mesh_sub(l.position,p);double r=mulib_mesh_length(d);if(!isfinite(r)||r<1e-9)return false;*direction=mulib_mesh_scale(d,1/r);*radiance=mulib_mesh_scale(l.intensity,1/(r*r));*distance=r;return true;}
#ifdef __cplusplus
}
#endif
#endif
