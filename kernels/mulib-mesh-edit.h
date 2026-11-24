/** @file mulib-mesh-edit.h
 * Single-face extrusion with cap and side walls.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No region extrusion, bevel or inset.
 */
#ifndef MULIB_MESH_EDIT_H
#define MULIB_MESH_EDIT_H
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
/* Extrude one triangle, replacing it with the cap and appending 6 side triangles. */
static inline bool mulib_mesh_edit_extrude(mulib_mesh *m,size_t face,mulib_mesh_vec3 offset){if(!mulib_mesh_valid(m)||face>=m->triangle_count||m->vertex_capacity-m->vertex_count<3||m->triangle_capacity-m->triangle_count<6||!mulib_mesh_finite(offset))return false;mulib_mesh_tri old=m->triangles[face];uint32_t v[3]={old.a,old.b,old.c},w[3];mulib_mesh_vec3 p[3];for(int i=0;i<3;i++){p[i]=mulib_mesh_add(m->vertices[v[i]],offset);if(!mulib_mesh_finite(p[i]))return false;}for(int i=0;i<3;i++)mulib_mesh_append_vertex(m,p[i],&w[i]);mulib_mesh_tri t={w[0],w[1],w[2]};m->triangles[face]=t;for(int i=0;i<3;i++){int j=(i+1)%3;mulib_mesh_append_tri(m,v[i],v[j],w[j]);mulib_mesh_append_tri(m,v[i],w[j],w[i]);}return true;}
#ifdef __cplusplus
}
#endif
#endif
