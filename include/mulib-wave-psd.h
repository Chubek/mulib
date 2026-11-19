/* MuLIB / Amb3r: wave-psd. C11 single-header kernel.
 * Define MULIB_IMPL_WAVE_PSD in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_WAVE_PSD_H
#define MULIB_WAVE_PSD_H
#include "mulib-amb3r-base.h"
#include "mulib-wave-spectrum.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_wave_psd(const double *input,size_t n,size_t segment,size_t overlap,double sample_rate,double *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WAVE_PSD) && !defined(MULIB_WAVE_PSD_H_IMPLEMENTED)
#define MULIB_WAVE_PSD_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_wave_psd(const double *input,size_t n,size_t segment,size_t overlap,double sample_rate,double *out){if(!ma_finite(input,n)||segment<3||segment>n||overlap>=segment||sample_rate<=0||!isfinite(sample_rate)||!out)return 0;ma_complex *x=(ma_complex*)ma_alloc(segment,sizeof *x),*y=(ma_complex*)ma_alloc(segment,sizeof *y);if(!x||!y){free(x);free(y);return 0;}size_t bins=segment/2+1,step=segment-overlap,count=0;memset(out,0,bins*sizeof(double));double win=0;for(size_t i=0;i<segment;i++){double w=.5-.5*cos(2*MA_PI*i/(segment-1));win+=w*w;}for(size_t start=0;start<=n-segment;){for(size_t i=0;i<segment;i++)x[i]=(ma_complex){input[start+i]*(.5-.5*cos(2*MA_PI*i/(segment-1))),0};if(!mulib_wave_spectrum(x,segment,y,0)){free(x);free(y);return 0;}for(size_t i=0;i<bins;i++){double k=(i&&!(segment%2==0&&i==segment/2))?2:1;out[i]+=k*(y[i].re*y[i].re+y[i].im*y[i].im)/(sample_rate*win);}count++;if(step>n-segment-start)break;start+=step;}for(size_t i=0;i<bins;i++)out[i]/=count;free(x);free(y);return ma_finite(out,bins);}
#ifdef __cplusplus
}
#endif
#endif
