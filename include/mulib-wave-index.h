/* MuLIB / Amb3r: wave-index. C11 single-header kernel.
 * Define MULIB_IMPL_WAVE_INDEX in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_WAVE_INDEX_H
#define MULIB_WAVE_INDEX_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

size_t mulib_wave_index_lower(const double *times,size_t n,double t);
int mulib_wave_index_range(const double *times,size_t n,double from,double to,size_t *start,size_t *end);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WAVE_INDEX) && !defined(MULIB_WAVE_INDEX_H_IMPLEMENTED)
#define MULIB_WAVE_INDEX_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_wave_index_lower(const double *times,size_t n,double t){if((!times&&n)||!isfinite(t))return MA_NONE;size_t lo=0,hi=n;while(lo<hi){size_t mid=lo+(hi-lo)/2;if(times[mid]<t)lo=mid+1;else hi=mid;}return lo;}
int mulib_wave_index_range(const double *times,size_t n,double from,double to,size_t *start,size_t *end){if(!start||!end||from>to||!ma_finite(times,n))return 0;for(size_t i=1;i<n;i++)if(times[i]<=times[i-1])return 0;*start=mulib_wave_index_lower(times,n,from);*end=mulib_wave_index_lower(times,n,to);return *start!=MA_NONE&&*end!=MA_NONE;}
#ifdef __cplusplus
}
#endif
#endif
