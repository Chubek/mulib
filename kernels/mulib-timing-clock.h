/* MuLIB: timing-clock. C11 single-header kernel.
 * Define MULIB_IMPL_TIMING_CLOCK in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_TIMING_CLOCK_H
#define MULIB_TIMING_CLOCK_H
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
#include "mulib-storage-core.h"
#include "mulib-num-core.h"
#include "mulib-geom2-core.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_timing_clock_skew(const double *arrival,size_t n,double *skew);
double mulib_timing_clock_setup(double period,double launch,double capture,double path,double setup,double uncertainty);
double mulib_timing_clock_hold(double launch,double capture,double path,double hold,double uncertainty);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TIMING_CLOCK) && !defined(MULIB_TIMING_CLOCK_H_IMPLEMENTED)
#define MULIB_TIMING_CLOCK_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_timing_clock_skew(const double *arrival,size_t n,double *skew){if(!n||!mulib_num_all_finite(arrival,n)||!skew)return 0;double lo=arrival[0],hi=lo;for(size_t i=1;i<n;i++){lo=MULIB_STORAGE_MIN(lo,arrival[i]);hi=MULIB_STORAGE_MAX(hi,arrival[i]);}*skew=hi-lo;return isfinite(*skew);}
double mulib_timing_clock_setup(double period,double launch,double capture,double path,double setup,double uncertainty){return period+capture-launch-path-setup-uncertainty;}
double mulib_timing_clock_hold(double launch,double capture,double path,double hold,double uncertainty){return launch+path-capture-hold-uncertainty;}
#ifdef __cplusplus
}
#endif
#endif
