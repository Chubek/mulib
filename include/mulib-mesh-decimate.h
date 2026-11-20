/** @file mulib-mesh-decimate.h
 * Shortest-edge selection and index collapse with degenerate face removal.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Not QEM; may change topology; leaves unused vertices.
 */
#ifndef MULIB_MESH_DECIMATE_H
#define MULIB_MESH_DECIMATE_H
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
static inline bool mulib_mesh_decimate_shortest(const mulib_mesh *m,uint32_t *a,uint32_t *b,double *length){if(!mulib_mesh_valid(m)||!a||!b||!length)return false;double best=DBL_MAX;for(size_t i=0;i<m->triangle_count;i++){mulib_mesh_tri t=m->triangles[i];uint32_t v[3]={t.a,t.b,t.c};for(int k=0;k<3;k++){uint32_t u=v[k],w=v[(k+1)%3];double l=mulib_mesh_length(mulib_mesh_sub(m->vertices[u],m->vertices[w]));if(u!=w&&l<best){best=l;*a=u;*b=w;}}}if(best==DBL_MAX)return false;*length=best;return true;}
static inline bool mulib_mesh_decimate_collapse(mulib_mesh *m,uint32_t keep,uint32_t drop){if(!mulib_mesh_valid(m)||keep>=m->vertex_count||drop>=m->vertex_count||keep==drop)return false;size_t n=0;for(size_t i=0;i<m->triangle_count;i++){mulib_mesh_tri t=m->triangles[i];if(t.a==drop)t.a=keep;if(t.b==drop)t.b=keep;if(t.c==drop)t.c=keep;if(t.a!=t.b&&t.b!=t.c&&t.a!=t.c)m->triangles[n++]=t;}m->triangle_count=n;return true;}
#ifdef __cplusplus
}
#endif
#endif
