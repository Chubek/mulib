/* MuLIB: dsim-sequential. C11 single-header kernel.
 * Define MULIB_IMPL_DSIM_SEQUENTIAL in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_DSIM_SEQUENTIAL_H
#define MULIB_DSIM_SEQUENTIAL_H
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
#include "mulib-dsim-resolve.h"
#ifdef __cplusplus
extern "C" {
#endif

unsigned mulib_dsim_sequential_dff(unsigned old_clock,unsigned clock,unsigned d,unsigned q,unsigned reset,unsigned reset_value);
unsigned mulib_dsim_sequential_latch(unsigned enable,unsigned d,unsigned prior);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DSIM_SEQUENTIAL) && !defined(MULIB_DSIM_SEQUENTIAL_H_IMPLEMENTED)
#define MULIB_DSIM_SEQUENTIAL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
unsigned mulib_dsim_sequential_dff(unsigned old_clock,unsigned clock,unsigned d,unsigned q,unsigned reset,unsigned reset_value){if(reset==MULIB_DSIM_LOGIC_1)return reset_value;if(reset!=MULIB_DSIM_LOGIC_0)return MULIB_DSIM_LOGIC_X;if(old_clock==MULIB_DSIM_LOGIC_0&&clock==MULIB_DSIM_LOGIC_1)return d==MULIB_DSIM_LOGIC_Z?MULIB_DSIM_LOGIC_X:d;return q;}
unsigned mulib_dsim_sequential_latch(unsigned enable,unsigned d,unsigned prior){if(enable==MULIB_DSIM_LOGIC_1)return d==MULIB_DSIM_LOGIC_Z?MULIB_DSIM_LOGIC_X:d;if(enable==MULIB_DSIM_LOGIC_0)return prior;return d==prior?prior:MULIB_DSIM_LOGIC_X;}
#ifdef __cplusplus
}
#endif
#endif
