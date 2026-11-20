/** @file mulib-geom-deform.h
 * Twist, taper and cylindrical bend point deformation.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_GEOM_DEFORM_H
#define MULIB_GEOM_DEFORM_H
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
static inline mulib_mesh_vec3 mulib_geom_deform_twist(mulib_mesh_vec3 p,double radians_per_z){double a=p.z*radians_per_z,c=cos(a),s=sin(a);return mulib_mesh_v(c*p.x-s*p.y,s*p.x+c*p.y,p.z);}
static inline mulib_mesh_vec3 mulib_geom_deform_taper(mulib_mesh_vec3 p,double slope){double k=1+p.z*slope;return mulib_mesh_v(k*p.x,k*p.y,p.z);}
static inline mulib_mesh_vec3 mulib_geom_deform_bend(mulib_mesh_vec3 p,double k){if(fabs(k)<1e-12)return p;double a=p.z*k,r=1/k+p.x;return mulib_mesh_v(r*cos(a)-1/k,p.y,r*sin(a));}
#ifdef __cplusplus
}
#endif
#endif
