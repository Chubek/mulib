/* MuLIB / Amb3r: wave-trigger. C11 single-header kernel.
 * Define MULIB_IMPL_WAVE_TRIGGER in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_WAVE_TRIGGER_H
#define MULIB_WAVE_TRIGGER_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

size_t mulib_wave_trigger(const double *values,size_t n,double threshold,int rising,size_t start);
int mulib_wave_trigger_time(const double *times,const double *values,size_t n,size_t index,double threshold,double *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WAVE_TRIGGER) && !defined(MULIB_WAVE_TRIGGER_H_IMPLEMENTED)
#define MULIB_WAVE_TRIGGER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_wave_trigger(const double *values,size_t n,double threshold,int rising,size_t start){if(!values||!isfinite(threshold)||start>=n)return MA_NONE;for(size_t i=MA_MAX(start,1);i<n;i++)if(rising?(values[i-1]<threshold&&values[i]>=threshold):(values[i-1]>threshold&&values[i]<=threshold))return i;return MA_NONE;}
int mulib_wave_trigger_time(const double *times,const double *values,size_t n,size_t index,double threshold,double *out){if(!times||!values||!out||!index||index>=n||times[index]<=times[index-1]||values[index]==values[index-1])return 0;double u=(threshold-values[index-1])/(values[index]-values[index-1]);if(u<0||u>1||!isfinite(u))return 0;*out=times[index-1]+u*(times[index]-times[index-1]);return isfinite(*out);}
#ifdef __cplusplus
}
#endif
#endif
