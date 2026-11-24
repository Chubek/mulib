/** @file mulib-compute-cpu.h
 * Bounded deterministic CPU workgroup dispatch.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Serial; use taskpool for parallelism.
 */
#ifndef MULIB_COMPUTE_CPU_H
#define MULIB_COMPUTE_CPU_H
#include "mulib-compute-core.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef bool (*mulib_compute_cpu_work)(void*,uint32_t,uint32_t,uint32_t);
static inline mulib_media_status mulib_compute_cpu_dispatch(mulib_compute_grid g,mulib_compute_cpu_work work,void *ctx,uint64_t limit){if(!work||!g.x||!g.y||!g.z)return MULIB_MEDIA_INVALID;uint64_t xy=(uint64_t)g.x*g.y;if(xy>limit/g.z)return MULIB_MEDIA_RANGE;for(uint32_t z=0;z<g.z;z++)for(uint32_t y=0;y<g.y;y++)for(uint32_t x=0;x<g.x;x++)if(!work(ctx,x,y,z))return MULIB_MEDIA_CANCELLED;return MULIB_MEDIA_OK;}
#ifdef __cplusplus
}
#endif
#endif
