/* MuLIB / Amb3r: spice-noise. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_NOISE in exactly one implementation translation unit.
 * Profile: Noise-source formulas and uncorrelated transfer aggregation; not whole-circuit noise solve.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_NOISE_H
#define MULIB_SPICE_NOISE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

double mulib_spice_noise_resistor(double resistance,double kelvin);
double mulib_spice_noise_shot(double current);
double mulib_spice_noise_output(const ma_complex *transfer,const double *density,size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_NOISE) && !defined(MULIB_SPICE_NOISE_H_IMPLEMENTED)
#define MULIB_SPICE_NOISE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
double mulib_spice_noise_resistor(double resistance,double kelvin){if(resistance<=0||kelvin<=0)return NAN;return 4*1.380649e-23*kelvin*resistance;}
double mulib_spice_noise_shot(double current){return 2*1.602176634e-19*fabs(current);}
double mulib_spice_noise_output(const ma_complex *transfer,const double *density,size_t n){if((!transfer&&n)||(!density&&n))return NAN;double s=0;for(size_t i=0;i<n;i++){if(density[i]<0||!isfinite(density[i]))return NAN;double m=ma_cabs(transfer[i]);s+=m*m*density[i];}return s;}
#ifdef __cplusplus
}
#endif
#endif
