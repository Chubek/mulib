/* MuLIB / Amb3r: spice-device. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_DEVICE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_DEVICE_H
#define MULIB_SPICE_DEVICE_H
#include "mulib-amb3r-base.h"
#include "mulib-spice-mna.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef int (*ma_device_stamp)(void *,ma_mna *,const double *,double);
typedef struct {ma_device_stamp stamp;void *context;} ma_spice_device;
int mulib_spice_device_stamp(const ma_spice_device *devices,size_t n,ma_mna *m,const double *x,double time);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_DEVICE) && !defined(MULIB_SPICE_DEVICE_H_IMPLEMENTED)
#define MULIB_SPICE_DEVICE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_device_stamp(const ma_spice_device *devices,size_t n,ma_mna *m,const double *x,double time){if((!devices&&n)||!m||!x||!isfinite(time))return 0;for(size_t i=0;i<n;i++)if(!devices[i].stamp||!devices[i].stamp(devices[i].context,m,x,time))return 0;return 1;}
#ifdef __cplusplus
}
#endif
#endif
