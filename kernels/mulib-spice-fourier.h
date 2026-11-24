/* MuLIB: spice-fourier. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_FOURIER in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_FOURIER_H
#define MULIB_SPICE_FOURIER_H
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

int mulib_spice_fourier(const double *samples,size_t n,size_t harmonics,mulib_num_complex *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_FOURIER) && !defined(MULIB_SPICE_FOURIER_H_IMPLEMENTED)
#define MULIB_SPICE_FOURIER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_fourier(const double *samples,size_t n,size_t harmonics,mulib_num_complex *out){if(!n||!mulib_num_all_finite(samples,n)||!out||harmonics>n/2+1)return 0;for(size_t k=0;k<harmonics;k++){double re=0,im=0;for(size_t j=0;j<n;j++){double a=2*MULIB_NUM_PI*(double)k*j/n;re+=samples[j]*cos(a);im-=samples[j]*sin(a);}out[k]=(mulib_num_complex){re/n,im/n};}return 1;}
#ifdef __cplusplus
}
#endif
#endif
