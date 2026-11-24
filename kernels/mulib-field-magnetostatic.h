/* MuLIB: field-magnetostatic. C11 single-header kernel.
 * Define MULIB_IMPL_FIELD_MAGNETOSTATIC in exactly one implementation translation unit.
 * Profile: 2-D scalar vector-potential solve, uniform permeability.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FIELD_MAGNETOSTATIC_H
#define MULIB_FIELD_MAGNETOSTATIC_H
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
#include "mulib-field-electrostatic.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_field_magnetostatic(double *potential,const double *current_density,const unsigned char *fixed,size_t w,size_t h,double dx,double dy,double permeability,size_t limit,double tolerance);
int mulib_field_magnetostatic_flux(const double *potential,size_t w,size_t h,double dx,double dy,mulib_geom2_point *field);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FIELD_MAGNETOSTATIC) && !defined(MULIB_FIELD_MAGNETOSTATIC_H_IMPLEMENTED)
#define MULIB_FIELD_MAGNETOSTATIC_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_field_magnetostatic(double *potential,const double *current_density,const unsigned char *fixed,size_t w,size_t h,double dx,double dy,double permeability,size_t limit,double tolerance){size_t n;if(!mulib_storage_size_mul(w,h,&n)||!mulib_num_all_finite(current_density,n)||permeability<=0||!isfinite(permeability))return 0;double *rhs=(double*)mulib_storage_alloc(n,sizeof *rhs);if(!rhs)return 0;for(size_t i=0;i<n;i++)rhs[i]=permeability*current_density[i];int ok=mulib_field_fdm_poisson(potential,rhs,fixed,w,h,dx,dy,limit,tolerance);free(rhs);return ok;}
int mulib_field_magnetostatic_flux(const double *potential,size_t w,size_t h,double dx,double dy,mulib_geom2_point *field){if(!mulib_field_electrostatic_gradient(potential,w,h,dx,dy,field))return 0;for(size_t i=0;i<w*h;i++){double bx=-field[i].y;field[i].y=field[i].x;field[i].x=bx;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
