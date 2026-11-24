/** @file mulib-geom-tessellate.h
 * Regular parametric surface tessellation via callback.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Not adaptive; add curvature-based refinement and crack prevention.
 */
#ifndef MULIB_GEOM_TESSELLATE_H
#define MULIB_GEOM_TESSELLATE_H
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
typedef bool (*mulib_geom_surface_fn)(void*,double,double,mulib_mesh_vec3*);
static inline bool mulib_geom_tessellate(mulib_mesh *m,unsigned nu,unsigned nv,mulib_geom_surface_fn f,void *ctx){if(!mulib_mesh_valid(m)||m->vertex_count||m->triangle_count||!f||!nu||!nv||nu>4096||nv>4096||(size_t)(nu+1)*(nv+1)>m->vertex_capacity||2*(size_t)nu*nv>m->triangle_capacity)return false;for(unsigned y=0;y<=nv;y++)for(unsigned x=0;x<=nu;x++){mulib_mesh_vec3 p;if(!f(ctx,(double)x/nu,(double)y/nv,&p)||!mulib_mesh_append_vertex(m,p,NULL)){m->vertex_count=0;return false;}}for(unsigned y=0;y<nv;y++)for(unsigned x=0;x<nu;x++){uint32_t a=y*(nu+1)+x;mulib_mesh_append_tri(m,a,a+1,a+nu+1);mulib_mesh_append_tri(m,a+1,a+nu+2,a+nu+1);}return true;}
#ifdef __cplusplus
}
#endif
#endif
