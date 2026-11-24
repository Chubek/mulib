/** @file mulib-mesh-boolean.h
 * Point-in-closed-mesh parity classification, a boolean building block.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No mesh boolean output; edge degeneracies and robust intersections unresolved.
 */
#ifndef MULIB_MESH_BOOLEAN_H
#define MULIB_MESH_BOOLEAN_H
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
/* Odd/even ray test for closed meshes. Points on surfaces/edges are ambiguous. */
static inline bool mulib_mesh_boolean_contains(const mulib_mesh *m,mulib_mesh_vec3 p,bool *inside){if(!mulib_mesh_valid(m)||!inside||!mulib_mesh_finite(p))return false;mulib_mesh_vec3 d=mulib_mesh_v(1,.371390676,.129837);size_t hits=0;for(size_t i=0;i<m->triangle_count;i++){mulib_mesh_tri t=m->triangles[i];mulib_mesh_vec3 a=m->vertices[t.a],e1=mulib_mesh_sub(m->vertices[t.b],a),e2=mulib_mesh_sub(m->vertices[t.c],a),q=mulib_mesh_cross(d,e2),s=mulib_mesh_sub(p,a);double det=mulib_mesh_dot(e1,q);if(fabs(det)<1e-12)continue;double u=mulib_mesh_dot(s,q)/det;if(u<0||u>1)continue;mulib_mesh_vec3 r=mulib_mesh_cross(s,e1);double v=mulib_mesh_dot(d,r)/det,h=mulib_mesh_dot(e2,r)/det;if(v>=0&&u+v<=1&&h>1e-10)hits++;}*inside=(hits&1)!=0;return true;}
#ifdef __cplusplus
}
#endif
#endif
