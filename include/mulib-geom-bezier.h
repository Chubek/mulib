/** @file mulib-geom-bezier.h
 * Arbitrary-degree de Casteljau curve evaluation.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Scratch must have n elements; no surface API.
 */
#ifndef MULIB_GEOM_BEZIER_H
#define MULIB_GEOM_BEZIER_H
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
/* scratch_cap is vec3 elements; scratch may alias p, but not out. Scratch may
 * change on numeric failure; out is unchanged. Work is O(n*n), caller budgets n. */
static inline bool mulib_geom_bezier_eval(const mulib_mesh_vec3 *p,size_t n,double t,mulib_mesh_vec3 *scratch,size_t scratch_cap,mulib_mesh_vec3 *out){if(!p||!n||!scratch||!out||scratch_cap<n||!isfinite(t)||t<0||t>1||n>SIZE_MAX/sizeof *p||!mulib_mesh_disjoint(scratch,n*sizeof *scratch,out,sizeof *out))return false;for(size_t i=0;i<n;i++)if(!mulib_mesh_finite(p[i]))return false;memmove(scratch,p,n*sizeof *p);for(size_t k=n-1;k;k--)for(size_t i=0;i<k;i++){scratch[i]=mulib_mesh_add(mulib_mesh_scale(scratch[i],1-t),mulib_mesh_scale(scratch[i+1],t));if(!mulib_mesh_finite(scratch[i]))return false;}*out=scratch[0];return true;}
#ifdef __cplusplus
}
#endif
#endif
