/* MuLIB: field-electrostatic. C11 single-header kernel.
 * Define MULIB_IMPL_FIELD_ELECTROSTATIC in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FIELD_ELECTROSTATIC_H
#define MULIB_FIELD_ELECTROSTATIC_H
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
#include "mulib-field-fdm.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_field_electrostatic(double *potential,const double *charge,const unsigned char *fixed,size_t w,size_t h,double dx,double dy,double epsilon,size_t limit,double tolerance);
int mulib_field_electrostatic_gradient(const double *potential,size_t w,size_t h,double dx,double dy,mulib_geom2_point *field);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FIELD_ELECTROSTATIC) && !defined(MULIB_FIELD_ELECTROSTATIC_H_IMPLEMENTED)
#define MULIB_FIELD_ELECTROSTATIC_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_field_electrostatic(double *potential,const double *charge,const unsigned char *fixed,size_t w,size_t h,double dx,double dy,double epsilon,size_t limit,double tolerance){size_t n;if(!mulib_storage_size_mul(w,h,&n)||!mulib_num_all_finite(charge,n)||epsilon<=0||!isfinite(epsilon))return 0;double *rhs=(double*)mulib_storage_alloc(n,sizeof *rhs);if(!rhs)return 0;for(size_t i=0;i<n;i++)rhs[i]=charge[i]/epsilon;int ok=mulib_field_fdm_poisson(potential,rhs,fixed,w,h,dx,dy,limit,tolerance);free(rhs);return ok;}
int mulib_field_electrostatic_gradient(const double *potential,size_t w,size_t h,double dx,double dy,mulib_geom2_point *field){size_t n;if(w<2||h<2||!mulib_storage_size_mul(w,h,&n)||!mulib_num_all_finite(potential,n)||!field||dx<=0||dy<=0)return 0;for(size_t y=0;y<h;y++)for(size_t x=0;x<w;x++){size_t l=x?x-1:x,r=x+1<w?x+1:x,d=y?y-1:y,u=y+1<h?y+1:y;field[y*w+x]=(mulib_geom2_point){-(potential[y*w+r]-potential[y*w+l])/((r-l)*dx),-(potential[u*w+x]-potential[d*w+x])/((u-d)*dy)};}return 1;}
#ifdef __cplusplus
}
#endif
#endif
