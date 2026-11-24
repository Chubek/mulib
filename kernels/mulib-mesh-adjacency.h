/** @file mulib-mesh-adjacency.h
 * Vertex-to-face incidence query.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Linear scan; add CSR adjacency.
 */
#ifndef MULIB_MESH_ADJACENCY_H
#define MULIB_MESH_ADJACENCY_H
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
static inline size_t mulib_mesh_adjacency_faces(const mulib_mesh *m,uint32_t v,size_t *out,size_t cap){if(!mulib_mesh_valid(m)||v>=m->vertex_count)return 0;size_t n=0;for(size_t i=0;i<m->triangle_count;i++){mulib_mesh_tri t=m->triangles[i];if(t.a==v||t.b==v||t.c==v){if(out&&n<cap)out[n]=i;n++;}}return n;}
#ifdef __cplusplus
}
#endif
#endif
