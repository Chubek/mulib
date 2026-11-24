/** @file mulib-mesh-remesh.h
 * Jacobi Laplacian smoothing with fixed vertices.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Face-weighted neighbors; no isotropic split/collapse/flip remeshing.
 */
#ifndef MULIB_MESH_REMESH_H
#define MULIB_MESH_REMESH_H
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
/* One Jacobi step; scratch_cap is vec3 elements, fixed_count bool elements.
 * Optional fixed=NULL requires fixed_count=0. Spans are disjoint. Mesh remains
 * unchanged on failure; scratch may change. O(V*T), not isotropic remeshing. */
static inline bool mulib_mesh_remesh_smooth(mulib_mesh *m,mulib_mesh_vec3 *scratch,size_t scratch_cap,const bool *fixed,size_t fixed_count,double factor){
 if(!mulib_mesh_valid(m)||!scratch||scratch_cap<m->vertex_count||(!fixed&&fixed_count)||(fixed&&fixed_count<m->vertex_count)||m->triangle_count>SIZE_MAX/6||!isfinite(factor)||factor<0||factor>1)return false;
 size_t bytes=m->vertex_count*sizeof *scratch;
 if(!mulib_mesh_disjoint(scratch,bytes,m->vertices,bytes)||!mulib_mesh_disjoint(scratch,bytes,m->triangles,m->triangle_count*sizeof *m->triangles)||!mulib_mesh_disjoint(scratch,bytes,fixed,fixed?m->vertex_count*sizeof *fixed:0)||!mulib_mesh_disjoint(m->vertices,bytes,fixed,fixed?m->vertex_count*sizeof *fixed:0))return false;
 for(size_t i=0;i<m->vertex_count;i++){mulib_mesh_vec3 s=mulib_mesh_v(0,0,0);size_t n=0;for(size_t j=0;j<m->triangle_count;j++){mulib_mesh_tri t=m->triangles[j];uint32_t v[3]={t.a,t.b,t.c};for(int k=0;k<3;k++)if(v[k]==i){s=mulib_mesh_add(s,mulib_mesh_add(m->vertices[v[(k+1)%3]],m->vertices[v[(k+2)%3]]));n+=2;}}scratch[i]=n&&!(fixed&&fixed[i])?mulib_mesh_add(mulib_mesh_scale(m->vertices[i],1-factor),mulib_mesh_scale(s,factor/n)):m->vertices[i];if(!mulib_mesh_finite(scratch[i]))return false;}
 if(bytes)memcpy(m->vertices,scratch,bytes);
 return true;
}
#ifdef __cplusplus
}
#endif
#endif
