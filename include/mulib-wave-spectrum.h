/* MuLIB: wave-spectrum. C11 single-header kernel.
 * Define MULIB_IMPL_WAVE_SPECTRUM in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_WAVE_SPECTRUM_H
#define MULIB_WAVE_SPECTRUM_H
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

int mulib_wave_spectrum(const mulib_num_complex *input,size_t n,mulib_num_complex *out,int inverse);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WAVE_SPECTRUM) && !defined(MULIB_WAVE_SPECTRUM_H_IMPLEMENTED)
#define MULIB_WAVE_SPECTRUM_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_wave_spectrum(const mulib_num_complex *input,size_t n,mulib_num_complex *out,int inverse){if(!input||!out||!n)return 0;mulib_num_complex *a=(mulib_num_complex*)mulib_storage_alloc(n,sizeof *a);if(!a)return 0;for(size_t i=0;i<n;i++)if(!isfinite(input[i].re)||!isfinite(input[i].im)){free(a);return 0;}double sign=inverse?1:-1;if((n&(n-1))==0){memcpy(a,input,n*sizeof *a);for(size_t i=1,j=0;i<n;i++){size_t bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j){mulib_num_complex t=a[i];a[i]=a[j];a[j]=t;}}for(size_t len=2;len<=n;len*=2){size_t half=len/2;for(size_t i=0;i<n;i+=len)for(size_t j=0;j<half;j++){double ang=sign*2*MULIB_NUM_PI*j/len;mulib_num_complex z={cos(ang),sin(ang)},u=a[i+j],v=mulib_num_complex_mul(a[i+j+half],z);a[i+j]=mulib_num_complex_add(u,v);a[i+j+half]=mulib_num_complex_sub(u,v);}if(len==n)break;}}else{for(size_t k=0;k<n;k++){mulib_num_complex v={0,0};for(size_t j=0;j<n;j++){double ang=sign*2*MULIB_NUM_PI*(double)k*j/n;mulib_num_complex z={cos(ang),sin(ang)};v=mulib_num_complex_add(v,mulib_num_complex_mul(input[j],z));}a[k]=v;}}for(size_t i=0;i<n;i++){if(inverse){a[i].re/=n;a[i].im/=n;}if(!isfinite(a[i].re)||!isfinite(a[i].im)){free(a);return 0;}}memcpy(out,a,n*sizeof *a);free(a);return 1;}
#ifdef __cplusplus
}
#endif
#endif
