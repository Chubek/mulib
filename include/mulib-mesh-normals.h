/** @file mulib-mesh-normals.h
 * Area-weighted smooth vertex normals.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No tangent frames or split normals.
 */
#ifndef MULIB_MESH_NORMALS_H
#define MULIB_MESH_NORMALS_H
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
static inline bool mulib_mesh_normals(const mulib_mesh *m,mulib_mesh_vec3 *out,size_t cap){if(!mulib_mesh_valid(m)||!out||cap<m->vertex_count||out==m->vertices)return false;for(size_t i=0;i<m->vertex_count;i++)out[i]=mulib_mesh_v(0,0,0);for(size_t i=0;i<m->triangle_count;i++){mulib_mesh_tri t=m->triangles[i];mulib_mesh_vec3 n=mulib_mesh_cross(mulib_mesh_sub(m->vertices[t.b],m->vertices[t.a]),mulib_mesh_sub(m->vertices[t.c],m->vertices[t.a]));out[t.a]=mulib_mesh_add(out[t.a],n);out[t.b]=mulib_mesh_add(out[t.b],n);out[t.c]=mulib_mesh_add(out[t.c],n);}for(size_t i=0;i<m->vertex_count;i++)out[i]=mulib_mesh_unit(out[i]);return true;}
#ifdef __cplusplus
}
#endif
#endif
