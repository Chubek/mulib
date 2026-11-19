/* MuLIB / Amb3r: wave-jitter. C11 single-header kernel.
 * Define MULIB_IMPL_WAVE_JITTER in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_WAVE_JITTER_H
#define MULIB_WAVE_JITTER_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_wave_jitter(const double *edges,size_t n,double ideal_period,double *rms,double *peak_to_peak);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WAVE_JITTER) && !defined(MULIB_WAVE_JITTER_H_IMPLEMENTED)
#define MULIB_WAVE_JITTER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_wave_jitter(const double *edges,size_t n,double ideal_period,double *rms,double *peak_to_peak){if(n<2||!ma_finite(edges,n)||ideal_period<=0||!isfinite(ideal_period)||!rms||!peak_to_peak)return 0;double mean=0,m2=0,lo=INFINITY,hi=-INFINITY;for(size_t i=0;i<n;i++){if(i&&edges[i]<=edges[i-1])return 0;double e=edges[i]-edges[0]-ideal_period*i,d=e-mean;mean+=d/(i+1);m2+=d*(e-mean);lo=MA_MIN(lo,e);hi=MA_MAX(hi,e);}*rms=sqrt(m2/n);*peak_to_peak=hi-lo;return isfinite(*rms)&&isfinite(*peak_to_peak);}
#ifdef __cplusplus
}
#endif
#endif
