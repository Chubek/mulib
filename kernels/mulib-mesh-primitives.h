/** @file mulib-mesh-primitives.h
 * Box and regular grid primitive generation.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_MESH_PRIMITIVES_H
#define MULIB_MESH_PRIMITIVES_H
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
static inline bool mulib_mesh_primitives_box(mulib_mesh *m,mulib_mesh_vec3 half){if(!mulib_mesh_valid(m)||m->vertex_count||m->triangle_count||m->vertex_capacity<8||m->triangle_capacity<12||!mulib_mesh_finite(half)||half.x<=0||half.y<=0||half.z<=0)return false;for(unsigned i=0;i<8;i++)mulib_mesh_append_vertex(m,mulib_mesh_v(i&1?half.x:-half.x,i&2?half.y:-half.y,i&4?half.z:-half.z),NULL);static const uint32_t f[12][3]={{0,2,1},{1,2,3},{4,5,6},{5,7,6},{0,1,4},{1,5,4},{2,6,3},{3,6,7},{0,4,2},{2,4,6},{1,3,5},{3,7,5}};for(int i=0;i<12;i++)mulib_mesh_append_tri(m,f[i][0],f[i][1],f[i][2]);return true;}
static inline bool mulib_mesh_primitives_grid(mulib_mesh *m,unsigned nx,unsigned ny,double width,double height){if(!mulib_mesh_valid(m)||m->vertex_count||m->triangle_count||!nx||!ny||nx>4096||ny>4096||!isfinite(width)||!isfinite(height)||width<=0||height<=0||(size_t)(nx+1)*(ny+1)>m->vertex_capacity||2*(size_t)nx*ny>m->triangle_capacity)return false;for(unsigned y=0;y<=ny;y++)for(unsigned x=0;x<=nx;x++)mulib_mesh_append_vertex(m,mulib_mesh_v(width*((double)x/nx-.5),height*((double)y/ny-.5),0),NULL);for(unsigned y=0;y<ny;y++)for(unsigned x=0;x<nx;x++){uint32_t a=y*(nx+1)+x,b=a+1,c=a+nx+1,d=c+1;mulib_mesh_append_tri(m,a,b,c);mulib_mesh_append_tri(m,b,d,c);}return true;}
#ifdef __cplusplus
}
#endif
#endif
