/** @file mulib-render-pathtrace.h
 * Diffuse multi-bounce stochastic path tracing.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Validated scene required; no MIS, specular BSDF, Russian roulette or BVH integration.
 */
#ifndef MULIB_RENDER_PATHTRACE_H
#define MULIB_RENDER_PATHTRACE_H
#include "mulib-mesh-core.h"
#include "mulib-render-scene.h"
#include "mulib-render-sampling.h"
#include "mulib-render-bsdf.h"
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
/* Diffuse Monte Carlo transport with emissive triangles and environment. */
static inline mulib_mesh_vec3 mulib_render_pathtrace_sample(mulib_render_scene s,mulib_render_ray r,uint64_t *rng,unsigned bounces){mulib_mesh_vec3 out=mulib_mesh_v(0,0,0),weight=mulib_mesh_v(1,1,1);if(!rng||bounces>64)return out;for(unsigned i=0;i<=bounces;i++){mulib_render_hit h;if(!mulib_render_scene_hit(s,r,1e-6,DBL_MAX,&h)){out=mulib_mesh_add(out,mulib_render_bsdf_product(weight,s.background));break;}mulib_render_material m=s.materials[s.triangles[h.triangle].material];out=mulib_mesh_add(out,mulib_render_bsdf_product(weight,m.emission));if(i==bounces)break;weight=mulib_mesh_scale(mulib_render_bsdf_product(weight,m.albedo),1-m.metallic);double u=mulib_render_sampling_uniform(rng),v=mulib_render_sampling_uniform(rng);r.origin=mulib_mesh_add(mulib_mesh_add(r.origin,mulib_mesh_scale(r.direction,h.t)),mulib_mesh_scale(h.normal,1e-6));r.direction=mulib_render_sampling_cosine(h.normal,u,v);}return out;}
#ifdef __cplusplus
}
#endif
#endif
