/** @file mulib-mesh-halfedge.h
 * Triangle half-edge construction and nonmanifold edge rejection.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: O(E²), no nonmanifold vertex detection.
 */
#ifndef MULIB_MESH_HALFEDGE_H
#define MULIB_MESH_HALFEDGE_H
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
typedef struct {uint32_t from,to;size_t face,next,twin;} mulib_mesh_halfedge;
/* cap is halfedge elements, at least 3*triangle_count. Output is unchanged on
 * invalid topology/capacity. e must be disjoint from both mesh buffers. */
static inline bool mulib_mesh_halfedge_build(const mulib_mesh *m,mulib_mesh_halfedge *e,size_t cap){
 if(!mulib_mesh_valid(m)||m->triangle_count>cap/3||m->triangle_count>SIZE_MAX/3/sizeof *e||(!e&&m->triangle_count))return false;
 size_t n=3*m->triangle_count,bytes=n*sizeof *e;
 if(!mulib_mesh_disjoint(e,bytes,m->vertices,m->vertex_count*sizeof *m->vertices)||!mulib_mesh_disjoint(e,bytes,m->triangles,m->triangle_count*sizeof *m->triangles))return false;
 /* Validate before writing: >2 incident faces necessarily repeat a direction. */
 for(size_t i=0;i<m->triangle_count;i++){
  mulib_mesh_tri t=m->triangles[i];uint32_t v[3]={t.a,t.b,t.c};
  if(t.a==t.b||t.b==t.c||t.c==t.a)return false;
  for(size_t j=0;j<i;j++){mulib_mesh_tri u=m->triangles[j];uint32_t w[3]={u.a,u.b,u.c};for(size_t a=0;a<3;a++)for(size_t b=0;b<3;b++)if(v[a]==w[b]&&v[(a+1)%3]==w[(b+1)%3])return false;}
 }
 for(size_t i=0;i<m->triangle_count;i++){mulib_mesh_tri t=m->triangles[i];uint32_t v[3]={t.a,t.b,t.c};for(size_t k=0;k<3;k++){size_t j=3*i+k;e[j].from=v[k];e[j].to=v[(k+1)%3];e[j].face=i;e[j].next=3*i+(k+1)%3;e[j].twin=SIZE_MAX;}}
 for(size_t i=0;i<n;i++)for(size_t j=i+1;j<n;j++)if(e[i].from==e[j].to&&e[i].to==e[j].from){e[i].twin=j;e[j].twin=i;}
 return true;
}
#ifdef __cplusplus
}
#endif
#endif
