/** @file mulib-render-bsdf.h
 * Lambert BRDF, GGX normal distribution and Schlick Fresnel primitives.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No full GGX sampling, masking term or transmission.
 */
#ifndef MULIB_RENDER_BSDF_H
#define MULIB_RENDER_BSDF_H
#include "mulib-mesh-core.h"
#include "mulib-render-material.h"
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
static inline mulib_mesh_vec3 mulib_render_bsdf_product(mulib_mesh_vec3 a,mulib_mesh_vec3 b){return mulib_mesh_v(a.x*b.x,a.y*b.y,a.z*b.z);}
static inline mulib_mesh_vec3 mulib_render_bsdf_lambert(mulib_render_material m){return mulib_mesh_scale(m.albedo,(1-m.metallic)/3.141592653589793);}
static inline double mulib_render_bsdf_ggx_d(double ndoth,double roughness){double a=fmax(.001,roughness*roughness),a2=a*a,d=ndoth*ndoth*(a2-1)+1;return ndoth>0?a2/(3.141592653589793*d*d):0;}
static inline mulib_mesh_vec3 mulib_render_bsdf_fresnel(mulib_mesh_vec3 f0,double cos_theta){double k=pow(1-fmax(0,fmin(1,cos_theta)),5);return mulib_mesh_add(f0,mulib_mesh_scale(mulib_mesh_sub(mulib_mesh_v(1,1,1),f0),k));}
#ifdef __cplusplus
}
#endif
#endif
