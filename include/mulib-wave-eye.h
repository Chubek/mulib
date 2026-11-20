/* MuLIB: wave-eye. C11 single-header kernel.
 * Define MULIB_IMPL_WAVE_EYE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_WAVE_EYE_H
#define MULIB_WAVE_EYE_H
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
#include "mulib-graph-core.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_wave_eye(const double *times,const double *values,size_t n,double ui,double offset,double low,double high,size_t width,size_t height,uint64_t *bins);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WAVE_EYE) && !defined(MULIB_WAVE_EYE_H_IMPLEMENTED)
#define MULIB_WAVE_EYE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_wave_eye(const double *times,const double *values,size_t n,double ui,double offset,double low,double high,size_t width,size_t height,uint64_t *bins){size_t count;if(!mulib_num_all_finite(times,n)||!mulib_num_all_finite(values,n)||!isfinite(ui)||ui<=0||!isfinite(offset)||!isfinite(low)||!isfinite(high)||high<=low||!width||!height||!mulib_storage_size_mul(width,height,&count)||!bins)return 0;memset(bins,0,count*sizeof *bins);for(size_t i=0;i<n;i++){double phase=fmod(times[i]-offset,2*ui);if(phase<0)phase+=2*ui;if(values[i]<low||values[i]>high)continue;size_t x=(size_t)(phase/(2*ui)*width),y=(size_t)((values[i]-low)/(high-low)*height);if(x>=width)x=width-1;if(y>=height)y=height-1;size_t k=y*width+x;if(bins[k]!=UINT64_MAX)bins[k]++;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
