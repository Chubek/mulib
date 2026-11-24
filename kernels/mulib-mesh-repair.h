/** @file mulib-mesh-repair.h
 * Remove zero-area or below-threshold triangles.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No hole filling, welding or orientation repair.
 */
#ifndef MULIB_MESH_REPAIR_H
#define MULIB_MESH_REPAIR_H
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
static inline size_t mulib_mesh_repair_degenerate(mulib_mesh *m,double min_double_area){if(!mulib_mesh_valid(m)||!isfinite(min_double_area)||min_double_area<0)return 0;size_t n=0,old=m->triangle_count;for(size_t i=0;i<old;i++){mulib_mesh_tri t=m->triangles[i];double a=mulib_mesh_length(mulib_mesh_cross(mulib_mesh_sub(m->vertices[t.b],m->vertices[t.a]),mulib_mesh_sub(m->vertices[t.c],m->vertices[t.a])));if(isfinite(a)&&a>min_double_area)m->triangles[n++]=t;}m->triangle_count=n;return old-n;}
#ifdef __cplusplus
}
#endif
#endif
