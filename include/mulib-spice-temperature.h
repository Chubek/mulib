/* MuLIB / Amb3r: spice-temperature. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_TEMPERATURE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_TEMPERATURE_H
#define MULIB_SPICE_TEMPERATURE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

double mulib_spice_temperature_resistance(double r0,double t0,double t,double tc1,double tc2);
double mulib_spice_temperature_saturation(double is0,double t0,double t,double eg,double xti);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_TEMPERATURE) && !defined(MULIB_SPICE_TEMPERATURE_H_IMPLEMENTED)
#define MULIB_SPICE_TEMPERATURE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
double mulib_spice_temperature_resistance(double r0,double t0,double t,double tc1,double tc2){if(r0<=0||t0<=0||t<=0||!isfinite(r0+t0+t+tc1+tc2))return NAN;double d=t-t0,r=r0*(1+tc1*d+tc2*d*d);return r>0?r:NAN;}
double mulib_spice_temperature_saturation(double is0,double t0,double t,double eg,double xti){if(is0<=0||t0<=0||t<=0||eg<0)return NAN;return is0*pow(t/t0,xti)*exp(eg/8.617333262145e-5*(1/t0-1/t));}
#ifdef __cplusplus
}
#endif
#endif
