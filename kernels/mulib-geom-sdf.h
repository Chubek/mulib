/** @file mulib-geom-sdf.h
 * Sphere, box and smooth-union signed distance functions.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_GEOM_SDF_H
#define MULIB_GEOM_SDF_H
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
static inline double mulib_geom_sdf_sphere(mulib_mesh_vec3 p,mulib_mesh_vec3 c,double r){return mulib_mesh_length(mulib_mesh_sub(p,c))-r;}
static inline double mulib_geom_sdf_box(mulib_mesh_vec3 p,mulib_mesh_vec3 h){mulib_mesh_vec3 q=mulib_mesh_v(fabs(p.x)-h.x,fabs(p.y)-h.y,fabs(p.z)-h.z);return mulib_mesh_length(mulib_mesh_v(fmax(q.x,0),fmax(q.y,0),fmax(q.z,0)))+fmin(fmax(q.x,fmax(q.y,q.z)),0);}
static inline double mulib_geom_sdf_smooth_union(double a,double b,double k){if(!(k>0))return fmin(a,b);double h=fmax(k-fabs(a-b),0)/k;return fmin(a,b)-h*h*k*.25;}
#ifdef __cplusplus
}
#endif
#endif
