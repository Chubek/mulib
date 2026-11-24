/** @file mulib-mesh-core.h
 * Caller-owned indexed triangle mesh and vector math.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Triangle-only initial model; integrate canonical cgeom/linalg types later.
 */
#ifndef MULIB_MESH_CORE_H
#define MULIB_MESH_CORE_H
#include "mulib-media-core.h"
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
typedef struct {double x,y,z;} mulib_mesh_vec3;
typedef struct {uint32_t a,b,c;} mulib_mesh_tri;
typedef struct {mulib_mesh_vec3 *vertices;size_t vertex_count,vertex_capacity;mulib_mesh_tri *triangles;size_t triangle_count,triangle_capacity;} mulib_mesh;
static inline mulib_mesh_vec3 mulib_mesh_v(double x,double y,double z){mulib_mesh_vec3 v={x,y,z};return v;}
static inline mulib_mesh_vec3 mulib_mesh_add(mulib_mesh_vec3 a,mulib_mesh_vec3 b){return mulib_mesh_v(a.x+b.x,a.y+b.y,a.z+b.z);}
static inline mulib_mesh_vec3 mulib_mesh_sub(mulib_mesh_vec3 a,mulib_mesh_vec3 b){return mulib_mesh_v(a.x-b.x,a.y-b.y,a.z-b.z);}
static inline mulib_mesh_vec3 mulib_mesh_scale(mulib_mesh_vec3 a,double t){return mulib_mesh_v(a.x*t,a.y*t,a.z*t);}
static inline double mulib_mesh_dot(mulib_mesh_vec3 a,mulib_mesh_vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static inline mulib_mesh_vec3 mulib_mesh_cross(mulib_mesh_vec3 a,mulib_mesh_vec3 b){return mulib_mesh_v(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
static inline double mulib_mesh_length(mulib_mesh_vec3 a){return hypot(hypot(a.x,a.y),a.z);}
/* Scale first: normalization also handles finite subnormal/very large vectors. */
static inline mulib_mesh_vec3 mulib_mesh_unit(mulib_mesh_vec3 a){double s=fmax(fabs(a.x),fmax(fabs(a.y),fabs(a.z)));if(!isfinite(a.x)||!isfinite(a.y)||!isfinite(a.z)||!(s>0))return mulib_mesh_v(0,0,0);a=mulib_mesh_v(a.x/s,a.y/s,a.z/s);return mulib_mesh_scale(a,1/mulib_mesh_length(a));}
static inline bool mulib_mesh_finite(mulib_mesh_vec3 v){return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}
/* Byte-span checks use uintptr_t on the supported flat-address targets. They do
 * not prove allocation provenance: callers still supply actual live objects. */
static inline bool mulib_mesh_disjoint(const void *a,size_t an,const void *b,size_t bn){if(!an||!bn)return true;if(!a||!b)return false;uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;if(an>UINTPTR_MAX-x||bn>UINTPTR_MAX-y)return false;return x+an<=y||y+bn<=x;}
/* Capacities/counts are elements, not bytes. Buffers must not overlap. This
 * bootstrap double representation is NOT ABI-compatible with float linalg. */
static inline bool mulib_mesh_storage_valid(const mulib_mesh *m){return m&&m->vertex_count<=m->vertex_capacity&&m->triangle_count<=m->triangle_capacity&&m->vertex_capacity<=UINT32_MAX&&m->vertex_capacity<=SIZE_MAX/sizeof *m->vertices&&m->triangle_capacity<=SIZE_MAX/sizeof *m->triangles&&(!m->vertex_capacity||m->vertices)&&(!m->triangle_capacity||m->triangles)&&mulib_mesh_disjoint(m->vertices,m->vertex_capacity*sizeof *m->vertices,m->triangles,m->triangle_capacity*sizeof *m->triangles);}
static inline bool mulib_mesh_valid(const mulib_mesh *m){if(!mulib_mesh_storage_valid(m))return false;for(size_t i=0;i<m->vertex_count;i++)if(!mulib_mesh_finite(m->vertices[i]))return false;for(size_t i=0;i<m->triangle_count;i++){mulib_mesh_tri t=m->triangles[i];if(t.a>=m->vertex_count||t.b>=m->vertex_count||t.c>=m->vertex_count)return false;}return true;}
static inline bool mulib_mesh_append_vertex(mulib_mesh *m,mulib_mesh_vec3 v,uint32_t *id){if(!mulib_mesh_storage_valid(m)||m->vertex_count>=m->vertex_capacity||!mulib_mesh_finite(v))return false;if(id)*id=(uint32_t)m->vertex_count;m->vertices[m->vertex_count++]=v;return true;}
static inline bool mulib_mesh_append_tri(mulib_mesh *m,uint32_t a,uint32_t b,uint32_t c){if(!mulib_mesh_storage_valid(m)||m->triangle_count>=m->triangle_capacity||a>=m->vertex_count||b>=m->vertex_count||c>=m->vertex_count)return false;mulib_mesh_tri t={a,b,c};m->triangles[m->triangle_count++]=t;return true;}
#ifdef __cplusplus
}
#endif
#endif
