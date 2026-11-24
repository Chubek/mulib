/* MuLIB: rf-tolerance. C11 single-header kernel.
 * Define MULIB_IMPL_RF_TOLERANCE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_RF_TOLERANCE_H
#define MULIB_RF_TOLERANCE_H
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

int mulib_rf_tolerance_worst(const double *nominal,const double *relative,const double *sensitivity,size_t n,double *absolute_error,double *rss);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RF_TOLERANCE) && !defined(MULIB_RF_TOLERANCE_H_IMPLEMENTED)
#define MULIB_RF_TOLERANCE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_rf_tolerance_worst(const double *nominal,const double *relative,const double *sensitivity,size_t n,double *absolute_error,double *rss){if(!mulib_num_all_finite(nominal,n)||!mulib_num_all_finite(relative,n)||!mulib_num_all_finite(sensitivity,n)||!absolute_error||!rss)return 0;double worst=0,r=0;for(size_t i=0;i<n;i++){if(relative[i]<0)return 0;double d=fabs(nominal[i]*relative[i]*sensitivity[i]);worst+=d;r=hypot(r,d);}*absolute_error=worst;*rss=r;return isfinite(worst)&&isfinite(r);}
#ifdef __cplusplus
}
#endif
#endif
