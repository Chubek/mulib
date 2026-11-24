/* MuLIB: hwil-clock. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_CLOCK in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_CLOCK_H
#define MULIB_HWIL_CLOCK_H
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

int mulib_hwil_clock_edge(uint64_t time,uint64_t period,uint64_t phase,int rising,uint64_t *next);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_CLOCK) && !defined(MULIB_HWIL_CLOCK_H_IMPLEMENTED)
#define MULIB_HWIL_CLOCK_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_clock_edge(uint64_t time,uint64_t period,uint64_t phase,int rising,uint64_t *next){if(!period||phase>=period||!next)return 0;uint64_t offset=rising?0:period/2;if(phase>UINT64_MAX-offset)return 0;uint64_t first=phase+offset;if(time<first){*next=first;return 1;}uint64_t delta=period-(time-first)%period;if(time>UINT64_MAX-delta)return 0;*next=time+delta;return 1;}
#ifdef __cplusplus
}
#endif
#endif
