/* MuLIB: rf-network. C11 single-header kernel.
 * Define MULIB_IMPL_RF_NETWORK in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_RF_NETWORK_H
#define MULIB_RF_NETWORK_H
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

int mulib_rf_network_s_to_abcd(const mulib_num_complex s[4],double z0,mulib_num_complex t[4]);
void mulib_rf_network_cascade(const mulib_num_complex a[4],const mulib_num_complex b[4],mulib_num_complex out[4]);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RF_NETWORK) && !defined(MULIB_RF_NETWORK_H_IMPLEMENTED)
#define MULIB_RF_NETWORK_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_rf_network_s_to_abcd(const mulib_num_complex s[4],double z0,mulib_num_complex t[4]){if(!s||!t||z0<=0||!isfinite(z0)||mulib_num_complex_abs(s[1])<=DBL_MIN)return 0;mulib_num_complex one={1,0},two={2,0},z={z0,0},p=mulib_num_complex_add(one,s[0]),m=mulib_num_complex_sub(one,s[0]),q=mulib_num_complex_add(one,s[3]),n=mulib_num_complex_sub(one,s[3]),x=mulib_num_complex_mul(s[1],s[2]),den=mulib_num_complex_mul(two,s[1]);t[0]=mulib_num_complex_div(mulib_num_complex_add(mulib_num_complex_mul(p,n),x),den);t[1]=mulib_num_complex_div(mulib_num_complex_mul(z,mulib_num_complex_sub(mulib_num_complex_mul(p,q),x)),den);t[2]=mulib_num_complex_div(mulib_num_complex_sub(mulib_num_complex_mul(m,n),x),mulib_num_complex_mul(den,z));t[3]=mulib_num_complex_div(mulib_num_complex_add(mulib_num_complex_mul(m,q),x),den);for(size_t i=0;i<4;i++)if(!isfinite(t[i].re)||!isfinite(t[i].im))return 0;return 1;}
void mulib_rf_network_cascade(const mulib_num_complex a[4],const mulib_num_complex b[4],mulib_num_complex out[4]){mulib_num_complex t[4];for(size_t i=0;i<2;i++)for(size_t j=0;j<2;j++)t[2*i+j]=mulib_num_complex_add(mulib_num_complex_mul(a[2*i],b[j]),mulib_num_complex_mul(a[2*i+1],b[2+j]));memcpy(out,t,sizeof t);}
#ifdef __cplusplus
}
#endif
#endif
