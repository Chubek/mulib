/* MuLIB / Amb3r: dsim-driver. C11 single-header kernel.
 * Define MULIB_IMPL_DSIM_DRIVER in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_DSIM_DRIVER_H
#define MULIB_DSIM_DRIVER_H
#include "mulib-amb3r-base.h"
#include "mulib-dsim-resolve.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_dsim_driver_set(ma_logic_driver *d,size_t n,size_t index,unsigned value,unsigned strength,unsigned *resolved);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DSIM_DRIVER) && !defined(MULIB_DSIM_DRIVER_H_IMPLEMENTED)
#define MULIB_DSIM_DRIVER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_dsim_driver_set(ma_logic_driver *d,size_t n,size_t index,unsigned value,unsigned strength,unsigned *resolved){if(!d||index>=n||value>MA_LOGIC_Z||!resolved)return 0;d[index]=(ma_logic_driver){value,strength};*resolved=mulib_dsim_resolve(d,n);return 1;}
#ifdef __cplusplus
}
#endif
#endif
