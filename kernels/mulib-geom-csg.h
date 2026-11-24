/** @file mulib-geom-csg.h
 * Implicit-field CSG union, intersection and difference.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Not exact boundary-representation CSG.
 */
#ifndef MULIB_GEOM_CSG_H
#define MULIB_GEOM_CSG_H
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
typedef enum {MULIB_GEOM_CSG_UNION,MULIB_GEOM_CSG_INTERSECTION,MULIB_GEOM_CSG_DIFFERENCE} mulib_geom_csg_op;
static inline double mulib_geom_csg_combine(double a,double b,mulib_geom_csg_op op){switch(op){case MULIB_GEOM_CSG_UNION:return fmin(a,b);case MULIB_GEOM_CSG_INTERSECTION:return fmax(a,b);case MULIB_GEOM_CSG_DIFFERENCE:return fmax(a,-b);default:return NAN;}}
#ifdef __cplusplus
}
#endif
#endif
