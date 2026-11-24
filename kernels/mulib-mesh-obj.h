/** @file mulib-mesh-obj.h
 * Wavefront OBJ position and triangle export.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No import, material, normal or UV records.
 */
#ifndef MULIB_MESH_OBJ_H
#define MULIB_MESH_OBJ_H
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
static inline bool mulib_mesh_obj_write(FILE *f,const mulib_mesh *m){if(!f||!mulib_mesh_valid(m))return false;for(size_t i=0;i<m->vertex_count;i++){mulib_mesh_vec3 v=m->vertices[i];if(fprintf(f,"v %.17g %.17g %.17g\n",v.x,v.y,v.z)<0)return false;}for(size_t i=0;i<m->triangle_count;i++){mulib_mesh_tri t=m->triangles[i];if(fprintf(f,"f %llu %llu %llu\n",(unsigned long long)t.a+1,(unsigned long long)t.b+1,(unsigned long long)t.c+1)<0)return false;}return !ferror(f);}
#ifdef __cplusplus
}
#endif
#endif
