/* MuLIB: dsim-resolve. C11 single-header kernel.
 * Define MULIB_IMPL_DSIM_RESOLVE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_DSIM_RESOLVE_H
#define MULIB_DSIM_RESOLVE_H
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
#ifdef __cplusplus
extern "C" {
#endif
enum {MULIB_DSIM_LOGIC_0,MULIB_DSIM_LOGIC_1,MULIB_DSIM_LOGIC_X,MULIB_DSIM_LOGIC_Z};
typedef struct {unsigned value,strength;} mulib_dsim_logic_driver;
unsigned mulib_dsim_resolve(const mulib_dsim_logic_driver *drivers,size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DSIM_RESOLVE) && !defined(MULIB_DSIM_RESOLVE_H_IMPLEMENTED)
#define MULIB_DSIM_RESOLVE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
unsigned mulib_dsim_resolve(const mulib_dsim_logic_driver *drivers,size_t n){if(!drivers&&n)return MULIB_DSIM_LOGIC_X;unsigned strength=0,value=MULIB_DSIM_LOGIC_Z;for(size_t i=0;i<n;i++){unsigned v=drivers[i].value,s=drivers[i].strength;if(v>MULIB_DSIM_LOGIC_Z)return MULIB_DSIM_LOGIC_X;if(v==MULIB_DSIM_LOGIC_Z)continue;if(value==MULIB_DSIM_LOGIC_Z||s>strength){value=v;strength=s;}else if(s==strength&&v!=value)value=MULIB_DSIM_LOGIC_X;}return value;}
#ifdef __cplusplus
}
#endif
#endif
