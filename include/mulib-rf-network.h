/* MuLIB / Amb3r: rf-network. C11 single-header kernel.
 * Define MULIB_IMPL_RF_NETWORK in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_RF_NETWORK_H
#define MULIB_RF_NETWORK_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_rf_network_s_to_abcd(const ma_complex s[4],double z0,ma_complex t[4]);
void mulib_rf_network_cascade(const ma_complex a[4],const ma_complex b[4],ma_complex out[4]);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RF_NETWORK) && !defined(MULIB_RF_NETWORK_H_IMPLEMENTED)
#define MULIB_RF_NETWORK_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_rf_network_s_to_abcd(const ma_complex s[4],double z0,ma_complex t[4]){if(!s||!t||z0<=0||!isfinite(z0)||ma_cabs(s[1])<=DBL_MIN)return 0;ma_complex one={1,0},two={2,0},z={z0,0},p=ma_cadd(one,s[0]),m=ma_csub(one,s[0]),q=ma_cadd(one,s[3]),n=ma_csub(one,s[3]),x=ma_cmul(s[1],s[2]),den=ma_cmul(two,s[1]);t[0]=ma_cdiv(ma_cadd(ma_cmul(p,n),x),den);t[1]=ma_cdiv(ma_cmul(z,ma_csub(ma_cmul(p,q),x)),den);t[2]=ma_cdiv(ma_csub(ma_cmul(m,n),x),ma_cmul(den,z));t[3]=ma_cdiv(ma_cadd(ma_cmul(m,q),x),den);for(size_t i=0;i<4;i++)if(!isfinite(t[i].re)||!isfinite(t[i].im))return 0;return 1;}
void mulib_rf_network_cascade(const ma_complex a[4],const ma_complex b[4],ma_complex out[4]){ma_complex t[4];for(size_t i=0;i<2;i++)for(size_t j=0;j<2;j++)t[2*i+j]=ma_cadd(ma_cmul(a[2*i],b[j]),ma_cmul(a[2*i+1],b[2+j]));memcpy(out,t,sizeof t);}
#ifdef __cplusplus
}
#endif
#endif
