/* MuLIB / Amb3r: wave-histogram. C11 single-header kernel.
 * Define MULIB_IMPL_WAVE_HISTOGRAM in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_WAVE_HISTOGRAM_H
#define MULIB_WAVE_HISTOGRAM_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_wave_histogram(const double *values,size_t n,double low,double high,uint64_t *bins,size_t count,uint64_t *under,uint64_t *over);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WAVE_HISTOGRAM) && !defined(MULIB_WAVE_HISTOGRAM_H_IMPLEMENTED)
#define MULIB_WAVE_HISTOGRAM_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_wave_histogram(const double *values,size_t n,double low,double high,uint64_t *bins,size_t count,uint64_t *under,uint64_t *over){if(!ma_finite(values,n)||!isfinite(low)||!isfinite(high)||high<=low||!bins||!count||!under||!over)return 0;memset(bins,0,count*sizeof *bins);*under=*over=0;for(size_t i=0;i<n;i++){if(values[i]<low){(*under)++;continue;}if(values[i]>high){(*over)++;continue;}size_t k=(size_t)((values[i]-low)/(high-low)*count);if(k>=count)k=count-1;bins[k]++;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
