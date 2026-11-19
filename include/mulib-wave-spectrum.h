/* MuLIB / Amb3r: wave-spectrum. C11 single-header kernel.
 * Define MULIB_IMPL_WAVE_SPECTRUM in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_WAVE_SPECTRUM_H
#define MULIB_WAVE_SPECTRUM_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_wave_spectrum(const ma_complex *input,size_t n,ma_complex *out,int inverse);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WAVE_SPECTRUM) && !defined(MULIB_WAVE_SPECTRUM_H_IMPLEMENTED)
#define MULIB_WAVE_SPECTRUM_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_wave_spectrum(const ma_complex *input,size_t n,ma_complex *out,int inverse){if(!input||!out||!n)return 0;ma_complex *a=(ma_complex*)ma_alloc(n,sizeof *a);if(!a)return 0;for(size_t i=0;i<n;i++)if(!isfinite(input[i].re)||!isfinite(input[i].im)){free(a);return 0;}double sign=inverse?1:-1;if((n&(n-1))==0){memcpy(a,input,n*sizeof *a);for(size_t i=1,j=0;i<n;i++){size_t bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j){ma_complex t=a[i];a[i]=a[j];a[j]=t;}}for(size_t len=2;len<=n;len*=2){size_t half=len/2;for(size_t i=0;i<n;i+=len)for(size_t j=0;j<half;j++){double ang=sign*2*MA_PI*j/len;ma_complex z={cos(ang),sin(ang)},u=a[i+j],v=ma_cmul(a[i+j+half],z);a[i+j]=ma_cadd(u,v);a[i+j+half]=ma_csub(u,v);}if(len==n)break;}}else{for(size_t k=0;k<n;k++){ma_complex v={0,0};for(size_t j=0;j<n;j++){double ang=sign*2*MA_PI*(double)k*j/n;ma_complex z={cos(ang),sin(ang)};v=ma_cadd(v,ma_cmul(input[j],z));}a[k]=v;}}for(size_t i=0;i<n;i++){if(inverse){a[i].re/=n;a[i].im/=n;}if(!isfinite(a[i].re)||!isfinite(a[i].im)){free(a);return 0;}}memcpy(out,a,n*sizeof *a);free(a);return 1;}
#ifdef __cplusplus
}
#endif
#endif
