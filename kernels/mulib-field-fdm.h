/* MuLIB: field-fdm. C11 single-header kernel.
 * Define MULIB_IMPL_FIELD_FDM in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FIELD_FDM_H
#define MULIB_FIELD_FDM_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-storage-core.h"
#include "mulib-num-core.h"
#include "mulib-geom2-core.h"
#include "mulib-graph-core.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_field_fdm_poisson(double *u,const double *source,const unsigned char *fixed,size_t w,size_t h,double dx,double dy,size_t limit,double tolerance);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FIELD_FDM) && !defined(MULIB_FIELD_FDM_H_IMPLEMENTED)
#define MULIB_FIELD_FDM_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_field_fdm_poisson(double *u,const double *source,const unsigned char *fixed,size_t w,size_t h,double dx,double dy,size_t limit,double tolerance){size_t n;if(w<3||h<3||!mulib_storage_size_mul(w,h,&n)||!mulib_num_all_finite(u,n)||!mulib_num_all_finite(source,n)||!fixed||dx<=0||dy<=0||tolerance<=0||!isfinite(dx+dy+tolerance))return 0;for(size_t y=0;y<h;y++)for(size_t x=0;x<w;x++)if((!x||x==w-1||!y||y==h-1)&&!fixed[y*w+x])return 0;double a=1/(dx*dx),b=1/(dy*dy),den=2*(a+b);for(size_t it=0;it<limit;it++){double err=0;for(size_t y=1;y+1<h;y++)for(size_t x=1;x+1<w;x++){size_t i=y*w+x;if(fixed[i])continue;double q=(source[i]+a*(u[i-1]+u[i+1])+b*(u[i-w]+u[i+w]))/den;if(!isfinite(q))return 0;err=MULIB_STORAGE_MAX(err,fabs(q-u[i]));u[i]=q;}if(err<=tolerance)return 1;}return 0;}
#ifdef __cplusplus
}
#endif
#endif
