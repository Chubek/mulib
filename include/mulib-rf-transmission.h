/* MuLIB / Amb3r: rf-transmission. C11 single-header kernel.
 * Define MULIB_IMPL_RF_TRANSMISSION in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_RF_TRANSMISSION_H
#define MULIB_RF_TRANSMISSION_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_rf_transmission_input(ma_complex load,double z0,double beta_length,ma_complex *out);
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
int mulib_rf_transmission_input(ma_complex load,double z0,double beta_length,ma_complex *out){if(!out||z0<=0||!isfinite(z0+beta_length+load.re+load.im))return 0;double c=cos(beta_length),s=sin(beta_length);ma_complex a=ma_cadd(ma_cmul(load,(ma_complex){c,0}),(ma_complex){0,z0*s}),b=ma_cadd((ma_complex){z0*c,0},ma_cmul(load,(ma_complex){0,s}));if(ma_cabs(b)<=DBL_MIN)return 0;*out=ma_cmul((ma_complex){z0,0},ma_cdiv(a,b));return isfinite(out->re)&&isfinite(out->im);}
double mulib_rf_transmission_velocity(double epsilon_r){return epsilon_r>=1?299792458.0/sqrt(epsilon_r):NAN;}
#ifdef __cplusplus
}
#endif
#endif
