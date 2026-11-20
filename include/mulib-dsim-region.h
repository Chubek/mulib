/* MuLIB: dsim-region. C11 single-header kernel.
 * Define MULIB_IMPL_DSIM_REGION in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_DSIM_REGION_H
#define MULIB_DSIM_REGION_H
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
enum {MULIB_DSIM_SIM_PREPONED,MULIB_DSIM_SIM_ACTIVE,MULIB_DSIM_SIM_INACTIVE,MULIB_DSIM_SIM_NBA,MULIB_DSIM_SIM_OBSERVED,MULIB_DSIM_SIM_REACTIVE,MULIB_DSIM_SIM_POSTPONED};
int mulib_dsim_region_next(uint64_t now,uint64_t delta,unsigned region,unsigned requested,uint64_t *next_time,uint64_t *next_delta);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DSIM_REGION) && !defined(MULIB_DSIM_REGION_H_IMPLEMENTED)
#define MULIB_DSIM_REGION_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_dsim_region_next(uint64_t now,uint64_t delta,unsigned region,unsigned requested,uint64_t *next_time,uint64_t *next_delta){if(region>MULIB_DSIM_SIM_POSTPONED||requested>MULIB_DSIM_SIM_POSTPONED||!next_time||!next_delta)return 0;if(requested<region&&delta==UINT64_MAX)return 0;*next_time=now;*next_delta=delta+(requested<region);return 1;}
#ifdef __cplusplus
}
#endif
#endif
