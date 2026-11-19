/* MuLIB / Amb3r: ecad-power. C11 single-header kernel.
 * Define MULIB_IMPL_ECAD_POWER in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ECAD_POWER_H
#define MULIB_ECAD_POWER_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t net;double voltage,tolerance;int source;} ma_power_pin;
int mulib_ecad_power_check(const ma_power_pin *p,size_t n,uint64_t *bad);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ECAD_POWER) && !defined(MULIB_ECAD_POWER_H_IMPLEMENTED)
#define MULIB_ECAD_POWER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_power_check(const ma_power_pin *p,size_t n,uint64_t *bad){if(!p&&n)return 0;for(size_t i=0;i<n;i++){if(!p[i].net||!isfinite(p[i].voltage)||!isfinite(p[i].tolerance)||p[i].tolerance<0)return 0;if(p[i].source)continue;int found=0;for(size_t j=0;j<n;j++)if(p[j].net==p[i].net&&p[j].source&&fabs(p[j].voltage-p[i].voltage)<=p[i].tolerance)found=1;if(!found){if(bad)*bad=p[i].net;return 0;}}return 1;}
#ifdef __cplusplus
}
#endif
#endif
