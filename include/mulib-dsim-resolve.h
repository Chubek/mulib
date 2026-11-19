/* MuLIB / Amb3r: dsim-resolve. C11 single-header kernel.
 * Define MULIB_IMPL_DSIM_RESOLVE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_DSIM_RESOLVE_H
#define MULIB_DSIM_RESOLVE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {MA_LOGIC_0,MA_LOGIC_1,MA_LOGIC_X,MA_LOGIC_Z};
typedef struct {unsigned value,strength;} ma_logic_driver;
unsigned mulib_dsim_resolve(const ma_logic_driver *drivers,size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DSIM_RESOLVE) && !defined(MULIB_DSIM_RESOLVE_H_IMPLEMENTED)
#define MULIB_DSIM_RESOLVE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
unsigned mulib_dsim_resolve(const ma_logic_driver *drivers,size_t n){if(!drivers&&n)return MA_LOGIC_X;unsigned strength=0,value=MA_LOGIC_Z;for(size_t i=0;i<n;i++){unsigned v=drivers[i].value,s=drivers[i].strength;if(v>MA_LOGIC_Z)return MA_LOGIC_X;if(v==MA_LOGIC_Z)continue;if(value==MA_LOGIC_Z||s>strength){value=v;strength=s;}else if(s==strength&&v!=value)value=MA_LOGIC_X;}return value;}
#ifdef __cplusplus
}
#endif
#endif
