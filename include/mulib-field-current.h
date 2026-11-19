/* MuLIB / Amb3r: field-current. C11 single-header kernel.
 * Define MULIB_IMPL_FIELD_CURRENT in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FIELD_CURRENT_H
#define MULIB_FIELD_CURRENT_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_field_current_density(const ma_point *efield,const double *conductivity,size_t n,ma_point *j,double *power_density);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FIELD_CURRENT) && !defined(MULIB_FIELD_CURRENT_H_IMPLEMENTED)
#define MULIB_FIELD_CURRENT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_field_current_density(const ma_point *efield,const double *conductivity,size_t n,ma_point *j,double *power_density){if((!efield&&n)||(!conductivity&&n)||(!j&&n)||(!power_density&&n))return 0;for(size_t i=0;i<n;i++){if(!ma_point_valid(efield[i])||conductivity[i]<0||!isfinite(conductivity[i]))return 0;j[i]=(ma_point){conductivity[i]*efield[i].x,conductivity[i]*efield[i].y};power_density[i]=j[i].x*efield[i].x+j[i].y*efield[i].y;if(!ma_point_valid(j[i])||!isfinite(power_density[i]))return 0;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
