/* MuLIB / Amb3r: timing-slack. C11 single-header kernel.
 * Define MULIB_IMPL_TIMING_SLACK in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_TIMING_SLACK_H
#define MULIB_TIMING_SLACK_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_timing_slack(size_t n,const double *arrival,const double *required,double *slack,double *worst,double *total_negative);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TIMING_SLACK) && !defined(MULIB_TIMING_SLACK_H_IMPLEMENTED)
#define MULIB_TIMING_SLACK_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_timing_slack(size_t n,const double *arrival,const double *required,double *slack,double *worst,double *total_negative){if((!arrival&&n)||(!required&&n)||(!slack&&n)||!worst||!total_negative)return 0;*worst=INFINITY;*total_negative=0;for(size_t i=0;i<n;i++){if(isnan(arrival[i])||isnan(required[i]))return 0;slack[i]=isinf(required[i])||isinf(arrival[i])?INFINITY:required[i]-arrival[i];*worst=MA_MIN(*worst,slack[i]);if(slack[i]<0)*total_negative+=slack[i];}return 1;}
#ifdef __cplusplus
}
#endif
#endif
