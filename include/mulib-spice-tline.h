/* MuLIB / Amb3r: spice-tline. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_TLINE in exactly one implementation translation unit.
 * Profile: Lossless transmission-line ABCD frequency response only.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_TLINE_H
#define MULIB_SPICE_TLINE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_spice_tline_lossless(double z0,double delay,double frequency,ma_complex *a,ma_complex *b,ma_complex *c,ma_complex *d);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_TLINE) && !defined(MULIB_SPICE_TLINE_H_IMPLEMENTED)
#define MULIB_SPICE_TLINE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_tline_lossless(double z0,double delay,double frequency,ma_complex *a,ma_complex *b,ma_complex *c,ma_complex *d){if(z0<=0||delay<0||frequency<0||!a||!b||!c||!d||!isfinite(z0+delay+frequency))return 0;double t=2*MA_PI*frequency*delay;*a=*d=(ma_complex){cos(t),0};*b=(ma_complex){0,z0*sin(t)};*c=(ma_complex){0,sin(t)/z0};return 1;}
#ifdef __cplusplus
}
#endif
#endif
