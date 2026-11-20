/** @file mulib-render-integrator.h
 * Direct Lambert illumination from point lights with visibility.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Validated scene required; no specular direct lighting.
 */
#ifndef MULIB_RENDER_INTEGRATOR_H
#define MULIB_RENDER_INTEGRATOR_H
#include "mulib-mesh-core.h"
#include "mulib-render-shadow.h"
#include "mulib-render-light.h"
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
static inline mulib_mesh_vec3 mulib_render_integrator_direct(mulib_render_scene s,mulib_render_ray ray,const mulib_render_light *lights,size_t count){mulib_render_hit h;if(!mulib_render_scene_hit(s,ray,1e-6,DBL_MAX,&h))return s.background;mulib_render_material m=s.materials[s.triangles[h.triangle].material];mulib_mesh_vec3 out=m.emission,p=mulib_mesh_add(ray.origin,mulib_mesh_scale(ray.direction,h.t));if(!lights)return out;for(size_t i=0;i<count;i++){mulib_mesh_vec3 d,l;double dist;if(mulib_render_light_sample(lights[i],p,&d,&l,&dist)){double z=mulib_mesh_dot(h.normal,d);if(z>0&&mulib_render_shadow_visible(s,p,h.normal,d,dist))out=mulib_mesh_add(out,mulib_mesh_scale(mulib_render_bsdf_product(l,mulib_render_bsdf_lambert(m)),z));}}return out;}
#ifdef __cplusplus
}
#endif
#endif
