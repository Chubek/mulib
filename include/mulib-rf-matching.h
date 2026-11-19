/* MuLIB / Amb3r: rf-matching. C11 single-header kernel.
 * Define MULIB_IMPL_RF_MATCHING in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_RF_MATCHING_H
#define MULIB_RF_MATCHING_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_rf_matching_l(double source,double load,int highpass,double *series,double *shunt,int *shunt_at_load);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RF_MATCHING) && !defined(MULIB_RF_MATCHING_H_IMPLEMENTED)
#define MULIB_RF_MATCHING_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_rf_matching_l(double source,double load,int highpass,double *series,double *shunt,int *shunt_at_load){if(source<=0||load<=0||!series||!shunt||!shunt_at_load||!isfinite(source+load))return 0;if(source==load){*series=0;*shunt=INFINITY;*shunt_at_load=1;return 1;}double lo=MA_MIN(source,load),hi=MA_MAX(source,load),q=sqrt(hi/lo-1);*series=(highpass?-1:1)*q*lo;*shunt=(highpass?1:-1)*hi/q;*shunt_at_load=load>source;return 1;}
#ifdef __cplusplus
}
#endif
#endif
