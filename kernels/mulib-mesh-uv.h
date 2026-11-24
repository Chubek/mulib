/** @file mulib-mesh-uv.h
 * Normalized planar UV projection.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No seam generation, LSCM or atlas packing.
 */
#ifndef MULIB_MESH_UV_H
#define MULIB_MESH_UV_H
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
/* cap is doubles, >=2*vertex_count; output disjoint from mesh buffers. */
static inline bool mulib_mesh_uv_planar(const mulib_mesh *m,double *uv,size_t cap,unsigned axis){if(!mulib_mesh_valid(m)||!uv||axis>2||m->vertex_count>cap/2||m->vertex_count>SIZE_MAX/2/sizeof *uv||!m->vertex_count||!mulib_mesh_disjoint(uv,2*m->vertex_count*sizeof *uv,m->vertices,m->vertex_count*sizeof *m->vertices)||!mulib_mesh_disjoint(uv,2*m->vertex_count*sizeof *uv,m->triangles,m->triangle_count*sizeof *m->triangles))return false;double lo[2]={DBL_MAX,DBL_MAX},hi[2]={-DBL_MAX,-DBL_MAX};for(size_t i=0;i<m->vertex_count;i++){mulib_mesh_vec3 p=m->vertices[i];double x[3]={p.x,p.y,p.z};for(int k=0;k<2;k++){double z=x[(axis+1+k)%3];lo[k]=fmin(lo[k],z);hi[k]=fmax(hi[k],z);}}for(size_t i=0;i<m->vertex_count;i++){mulib_mesh_vec3 p=m->vertices[i];double x[3]={p.x,p.y,p.z};for(int k=0;k<2;k++){double den=hi[k]-lo[k],z=x[(axis+1+k)%3];uv[2*i+k]=hi[k]==lo[k]?0:isfinite(den)?(z-lo[k])/den:(z*.5-lo[k]*.5)/(hi[k]*.5-lo[k]*.5);}}return true;}
#ifdef __cplusplus
}
#endif
#endif
