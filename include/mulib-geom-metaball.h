/** @file mulib-geom-metaball.h
 * Compact-support polynomial metaball field.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_GEOM_METABALL_H
#define MULIB_GEOM_METABALL_H
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
typedef struct {mulib_mesh_vec3 center;double radius,weight;} mulib_geom_metaball;
static inline double mulib_geom_metaball_eval(const mulib_geom_metaball *a,size_t n,mulib_mesh_vec3 p){if(!a&&n)return NAN;double s=0;for(size_t i=0;i<n;i++){if(!isfinite(a[i].radius)||a[i].radius<=0||!isfinite(a[i].weight))return NAN;double q=mulib_mesh_length(mulib_mesh_sub(p,a[i].center))/a[i].radius;if(q<1){double z=1-q*q;s+=a[i].weight*z*z*z;}}return s;}
#ifdef __cplusplus
}
#endif
#endif
