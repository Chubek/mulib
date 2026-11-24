/* MuLIB: field-current. C11 single-header kernel.
 * Define MULIB_IMPL_FIELD_CURRENT in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FIELD_CURRENT_H
#define MULIB_FIELD_CURRENT_H
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
#include "mulib-geom2-core.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_field_current_density(const mulib_geom2_point *efield,const double *conductivity,size_t n,mulib_geom2_point *j,double *power_density);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FIELD_CURRENT) && !defined(MULIB_FIELD_CURRENT_H_IMPLEMENTED)
#define MULIB_FIELD_CURRENT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_field_current_density(const mulib_geom2_point *efield,const double *conductivity,size_t n,mulib_geom2_point *j,double *power_density){if((!efield&&n)||(!conductivity&&n)||(!j&&n)||(!power_density&&n))return 0;for(size_t i=0;i<n;i++){if(!mulib_geom2_point_valid(efield[i])||conductivity[i]<0||!isfinite(conductivity[i]))return 0;j[i]=(mulib_geom2_point){conductivity[i]*efield[i].x,conductivity[i]*efield[i].y};power_density[i]=j[i].x*efield[i].x+j[i].y*efield[i].y;if(!mulib_geom2_point_valid(j[i])||!isfinite(power_density[i]))return 0;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
