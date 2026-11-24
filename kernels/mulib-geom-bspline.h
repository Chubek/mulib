/** @file mulib-geom-bspline.h
 * De Boor B-spline curve evaluation and span search.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_GEOM_BSPLINE_H
#define MULIB_GEOM_BSPLINE_H
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
/* knot_count is double elements, at least n+degree+1; n is control points.
 * p<=32. Outputs unchanged on failure. No surface or exact-predicate API. */
static inline bool mulib_geom_bspline_span(const double *k,size_t knot_count,size_t n,unsigned p,double t,size_t *span){if(!k||!span||!n||p>=n||p>32||n>SIZE_MAX-p-1||n+p+1>knot_count||n+p+1>SIZE_MAX/sizeof *k||!isfinite(t))return false;for(size_t i=0;i<n+p+1;i++)if(!isfinite(k[i])||(i&&k[i]<k[i-1]))return false;if(t<k[p]||t>k[n]||k[p]>=k[n])return false;if(t==k[n]){size_t s=n-1;while(s>p&&k[s]==k[n])s--;*span=s;return true;}size_t lo=p,hi=n;while(lo+1<hi){size_t i=lo+(hi-lo)/2;if(t<k[i])hi=i;else lo=i;}*span=lo;return true;}
/* Reject unrepresentable differences rather than silently using NaN weights. */
static inline bool mulib_geom_bspline_alpha(double lo,double hi,double t,double *a){double d=hi-lo;if(!isfinite(d))return false;*a=d>0?(t-lo)/d:0;return isfinite(*a)&&*a>=0&&*a<=1;}
static inline bool mulib_geom_bspline_eval(const mulib_mesh_vec3 *v,size_t n,const double *k,size_t knot_count,unsigned p,double t,mulib_mesh_vec3 *out){size_t s;if(!v||!out||n>SIZE_MAX/sizeof *v||!mulib_geom_bspline_span(k,knot_count,n,p,t,&s))return false;mulib_mesh_vec3 d[33];for(unsigned j=0;j<=p;j++){d[j]=v[s-p+j];if(!mulib_mesh_finite(d[j]))return false;}for(unsigned r=1;r<=p;r++)for(unsigned j=p;j>=r;j--){size_t i=s-p+j;double a;if(!mulib_geom_bspline_alpha(k[i],k[i+p-r+1],t,&a))return false;d[j]=mulib_mesh_add(mulib_mesh_scale(d[j-1],1-a),mulib_mesh_scale(d[j],a));if(!mulib_mesh_finite(d[j]))return false;}*out=d[p];return true;}
#ifdef __cplusplus
}
#endif
#endif
