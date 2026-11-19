/* MuLIB / Amb3r: field-electrothermal. C11 single-header kernel.
 * Define MULIB_IMPL_FIELD_ELECTROTHERMAL in exactly one implementation translation unit.
 * Profile: Lumped resistor thermal model only; not multi-physics field solve.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FIELD_ELECTROTHERMAL_H
#define MULIB_FIELD_ELECTROTHERMAL_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_field_electrothermal_step(double temperature,double ambient,double r0,double t0,double alpha,double voltage,double thermal_r,double thermal_c,double dt,double *next,double *power);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FIELD_ELECTROTHERMAL) && !defined(MULIB_FIELD_ELECTROTHERMAL_H_IMPLEMENTED)
#define MULIB_FIELD_ELECTROTHERMAL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_field_electrothermal_step(double temperature,double ambient,double r0,double t0,double alpha,double voltage,double thermal_r,double thermal_c,double dt,double *next,double *power){if(!next||!power||r0<=0||thermal_r<=0||thermal_c<=0||dt<=0||dt>thermal_r*thermal_c||!isfinite(temperature+ambient+r0+t0+alpha+voltage+thermal_r+thermal_c+dt))return 0;double r=r0*(1+alpha*(temperature-t0));if(r<=0)return 0;*power=voltage*voltage/r;*next=temperature+dt/thermal_c*(*power-(temperature-ambient)/thermal_r);return isfinite(*next)&&isfinite(*power);}
#ifdef __cplusplus
}
#endif
#endif
