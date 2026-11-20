/** @file mulib-geom-nurbs.h
 * Rational B-spline curve evaluation using homogeneous coordinates.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Positive weights, degree <=32; no tensor-product surface yet.
 */
#ifndef MULIB_GEOM_NURBS_H
#define MULIB_GEOM_NURBS_H
#include "mulib-mesh-core.h"
#include "mulib-geom-bspline.h"
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
/* n vec3 controls, weight_count doubles, knot_count doubles. Active weights
 * must be finite and positive. Intermediate overflow fails without changing out. */
static inline bool mulib_geom_nurbs_eval(const mulib_mesh_vec3 *v,const double *weights,size_t weight_count,size_t n,const double *k,size_t knot_count,unsigned p,double t,mulib_mesh_vec3 *out){size_t s;if(!v||!weights||!out||weight_count<n||n>SIZE_MAX/sizeof *v||!mulib_geom_bspline_span(k,knot_count,n,p,t,&s))return false;double d[33][4];for(unsigned j=0;j<=p;j++){size_t i=s-p+j;double w=weights[i];if(!isfinite(w)||w<=0||!mulib_mesh_finite(v[i]))return false;d[j][0]=v[i].x*w;d[j][1]=v[i].y*w;d[j][2]=v[i].z*w;d[j][3]=w;for(int q=0;q<4;q++)if(!isfinite(d[j][q]))return false;}for(unsigned r=1;r<=p;r++)for(unsigned j=p;j>=r;j--){size_t i=s-p+j;double a;if(!mulib_geom_bspline_alpha(k[i],k[i+p-r+1],t,&a))return false;for(int q=0;q<4;q++){d[j][q]=(1-a)*d[j-1][q]+a*d[j][q];if(!isfinite(d[j][q]))return false;}}if(!(d[p][3]>0))return false;mulib_mesh_vec3 x=mulib_mesh_v(d[p][0]/d[p][3],d[p][1]/d[p][3],d[p][2]/d[p][3]);if(!mulib_mesh_finite(x))return false;*out=x;return true;}
#ifdef __cplusplus
}
#endif
#endif
