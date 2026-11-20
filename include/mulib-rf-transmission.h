/* MuLIB: rf-transmission. C11 single-header kernel.
 * Define MULIB_IMPL_RF_TRANSMISSION in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_RF_TRANSMISSION_H
#define MULIB_RF_TRANSMISSION_H
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
#include "mulib-num-core.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_rf_transmission_input(mulib_num_complex load,double z0,double beta_length,mulib_num_complex *out);
double mulib_rf_transmission_velocity(double epsilon_r);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RF_TRANSMISSION) && !defined(MULIB_RF_TRANSMISSION_H_IMPLEMENTED)
#define MULIB_RF_TRANSMISSION_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_rf_transmission_input(mulib_num_complex load,double z0,double beta_length,mulib_num_complex *out){if(!out||z0<=0||!isfinite(z0+beta_length+load.re+load.im))return 0;double c=cos(beta_length),s=sin(beta_length);mulib_num_complex a=mulib_num_complex_add(mulib_num_complex_mul(load,(mulib_num_complex){c,0}),(mulib_num_complex){0,z0*s}),b=mulib_num_complex_add((mulib_num_complex){z0*c,0},mulib_num_complex_mul(load,(mulib_num_complex){0,s}));if(mulib_num_complex_abs(b)<=DBL_MIN)return 0;*out=mulib_num_complex_mul((mulib_num_complex){z0,0},mulib_num_complex_div(a,b));return isfinite(out->re)&&isfinite(out->im);}
double mulib_rf_transmission_velocity(double epsilon_r){return epsilon_r>=1?299792458.0/sqrt(epsilon_r):NAN;}
#ifdef __cplusplus
}
#endif
#endif
