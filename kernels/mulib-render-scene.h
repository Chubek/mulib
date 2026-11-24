/** @file mulib-render-scene.h
 * Triangle scene, material table and Moller-Trumbore closest-hit traversal.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Brute-force scene hit; validate scene once before render.
 */
#ifndef MULIB_RENDER_SCENE_H
#define MULIB_RENDER_SCENE_H
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
typedef struct {mulib_mesh_vec3 a,b,c;uint32_t material;} mulib_render_triangle;
typedef struct {const mulib_render_triangle *triangles;size_t count;const mulib_render_material *materials;size_t material_count;mulib_mesh_vec3 background;} mulib_render_scene;
typedef struct {mulib_mesh_vec3 origin,direction;} mulib_render_ray;
typedef struct {double t,u,v;size_t triangle;mulib_mesh_vec3 normal;} mulib_render_hit;
static inline bool mulib_render_scene_valid(mulib_render_scene s){if((s.count&&!s.triangles)||(s.material_count&&!s.materials)||!mulib_mesh_finite(s.background))return false;for(size_t i=0;i<s.material_count;i++)if(!mulib_render_material_valid(s.materials[i]))return false;for(size_t i=0;i<s.count;i++){mulib_render_triangle t=s.triangles[i];if(t.material>=s.material_count||!mulib_mesh_finite(t.a)||!mulib_mesh_finite(t.b)||!mulib_mesh_finite(t.c))return false;}return true;}
/* Finite coordinates/nonzero ray required; min/max may be infinite, not NaN.
 * Overflow/degeneracy is a miss, not robust exact intersection. Output preserved
 * on miss. triangle=SIZE_MAX here; scene/BVH traversal fills the actual index. */
static inline bool mulib_render_scene_tri_hit(mulib_render_triangle t,mulib_render_ray r,double min,double max,mulib_render_hit *out){
 if(!out||!mulib_mesh_finite(t.a)||!mulib_mesh_finite(t.b)||!mulib_mesh_finite(t.c)||!mulib_mesh_finite(r.origin)||!mulib_mesh_finite(r.direction)||!(min<=max)||(r.direction.x==0&&r.direction.y==0&&r.direction.z==0))return false;
 mulib_mesh_vec3 e1=mulib_mesh_sub(t.b,t.a),e2=mulib_mesh_sub(t.c,t.a),q=mulib_mesh_cross(r.direction,e2),s=mulib_mesh_sub(r.origin,t.a);
 double d=mulib_mesh_dot(e1,q);if(!isfinite(d)||fabs(d)<1e-12)return false;
 double u=mulib_mesh_dot(s,q)/d;if(!isfinite(u)||u<0||u>1)return false;
 mulib_mesh_vec3 p=mulib_mesh_cross(s,e1);double v=mulib_mesh_dot(r.direction,p)/d,h=mulib_mesh_dot(e2,p)/d;
 if(!isfinite(v)||v<0||u+v>1||h<min||h>max||!isfinite(h))return false;
 mulib_mesh_vec3 normal=mulib_mesh_cross(e1,e2);if(!mulib_mesh_finite(normal))return false;normal=mulib_mesh_unit(normal);if(mulib_mesh_length(normal)==0)return false;
 if(mulib_mesh_dot(normal,r.direction)>0)normal=mulib_mesh_scale(normal,-1);
 mulib_render_hit hit={h,u,v,SIZE_MAX,normal};*out=hit;return true;
}
static inline bool mulib_render_scene_hit(mulib_render_scene s,mulib_render_ray r,double min,double max,mulib_render_hit *out){if(!out||(!s.triangles&&s.count)||!mulib_mesh_finite(r.origin)||!mulib_mesh_finite(r.direction)||!(min<=max))return false;bool hit=false;for(size_t i=0;i<s.count;i++){mulib_render_hit h;if(mulib_render_scene_tri_hit(s.triangles[i],r,min,max,&h)){h.triangle=i;*out=h;max=h.t;hit=true;}}return hit;}
#ifdef __cplusplus
}
#endif
#endif
