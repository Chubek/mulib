/** @file mulib-mesh-subdivide.h
 * Shared-edge linear triangle subdivision.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Not Loop or Catmull-Clark smoothing; disjoint meshes.
 */
#ifndef MULIB_MESH_SUBDIVIDE_H
#define MULIB_MESH_SUBDIVIDE_H
#include "mulib-mesh-core.h"
#include "mulib-mesh-halfedge.h"
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
static inline uint32_t mulib_mesh_subdivide_mid(mulib_mesh *d,uint32_t a,uint32_t b,mulib_mesh_halfedge *e,uint32_t *ids,size_t edge){size_t twin=e[edge].twin;if(twin!=SIZE_MAX&&twin<edge)return ids[twin];uint32_t id=0;mulib_mesh_append_vertex(d,mulib_mesh_add(mulib_mesh_scale(d->vertices[a],.5),mulib_mesh_scale(d->vertices[b],.5)),&id);return id;}
/* Shared-edge midpoint subdivision (linear, not Loop smoothing). */
/* ecap/icap are halfedge/uint32 elements respectively, each at least 3*T.
 * All source, destination and scratch spans must be pairwise disjoint. Failure
 * preserves the destination (scratch may change). No geometric smoothing. */
static inline bool mulib_mesh_subdivide(const mulib_mesh *s,mulib_mesh *d,mulib_mesh_halfedge *e,size_t ecap,uint32_t *ids,size_t icap){
 if(!mulib_mesh_valid(s)||!mulib_mesh_valid(d)||s==d||d->vertex_count||d->triangle_count||s->triangle_count>ecap/3||s->triangle_count>icap/3||s->triangle_count>SIZE_MAX/3/sizeof *e||s->triangle_count>d->triangle_capacity/4||s->vertex_count>d->vertex_capacity)return false;
 size_t n=3*s->triangle_count;
 const void *ptr[6]={s->vertices,s->triangles,d->vertices,d->triangles,e,ids};
 size_t len[6]={s->vertex_count*sizeof *s->vertices,s->triangle_count*sizeof *s->triangles,d->vertex_capacity*sizeof *d->vertices,d->triangle_capacity*sizeof *d->triangles,n*sizeof *e,n*sizeof *ids};
 for(size_t i=0;i<6;i++){if(len[i]&&!ptr[i])return false;for(size_t j=0;j<i;j++)if(!mulib_mesh_disjoint(ptr[i],len[i],ptr[j],len[j]))return false;}
 if(!mulib_mesh_halfedge_build(s,e,ecap))return false;
 size_t need=0;for(size_t i=0;i<n;i++)if(e[i].twin==SIZE_MAX||e[i].twin>i)need++;
 if(need>d->vertex_capacity-s->vertex_count)return false;
 for(size_t i=0;i<s->vertex_count;i++)mulib_mesh_append_vertex(d,s->vertices[i],NULL);
 for(size_t i=0;i<n;i++)ids[i]=mulib_mesh_subdivide_mid(d,e[i].from,e[i].to,e,ids,i);
 for(size_t i=0;i<s->triangle_count;i++){mulib_mesh_tri t=s->triangles[i];uint32_t a=ids[3*i],b=ids[3*i+1],c=ids[3*i+2];mulib_mesh_append_tri(d,t.a,a,c);mulib_mesh_append_tri(d,t.b,b,a);mulib_mesh_append_tri(d,t.c,c,b);mulib_mesh_append_tri(d,a,b,c);}
 return true;
}
#ifdef __cplusplus
}
#endif
#endif
