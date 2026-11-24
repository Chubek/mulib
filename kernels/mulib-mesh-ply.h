/** @file mulib-mesh-ply.h
 * ASCII PLY positions and triangle export.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No binary PLY or import.
 */
#ifndef MULIB_MESH_PLY_H
#define MULIB_MESH_PLY_H
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
static inline bool mulib_mesh_ply_write(FILE *f,const mulib_mesh *m){if(!f||!mulib_mesh_valid(m)||fprintf(f,"ply\nformat ascii 1.0\nelement vertex %zu\nproperty double x\nproperty double y\nproperty double z\nelement face %zu\nproperty list uchar uint vertex_indices\nend_header\n",m->vertex_count,m->triangle_count)<0)return false;for(size_t i=0;i<m->vertex_count;i++){mulib_mesh_vec3 v=m->vertices[i];if(fprintf(f,"%.17g %.17g %.17g\n",v.x,v.y,v.z)<0)return false;}for(size_t i=0;i<m->triangle_count;i++){mulib_mesh_tri t=m->triangles[i];if(fprintf(f,"3 %u %u %u\n",(unsigned)t.a,(unsigned)t.b,(unsigned)t.c)<0)return false;}return !ferror(f);}
#ifdef __cplusplus
}
#endif
#endif
