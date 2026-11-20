/** @file mulib-compute-spirv.h
 * SPIR-V header/instruction framing validator and instruction emitter.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Structural only; not semantic verification or compiler.
 */
#ifndef MULIB_COMPUTE_SPIRV_H
#define MULIB_COMPUTE_SPIRV_H
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
static inline bool mulib_compute_spirv_validate(const uint32_t *w,size_t n){if(!w||n<5||n>SIZE_MAX/sizeof *w||w[0]!=0x07230203u||!w[3]||w[4]||((w[1]>>16)&255)!=1||((w[1]>>8)&255)>6||(w[1]&0xff0000ffu))return false;for(size_t i=5;i<n;){size_t count=w[i]>>16;if(!count||count>n-i)return false;i+=count;}return true;}
/* Capacities/counts are words; operands may overlap w, but n must be a
 * separate size_t object not overlapping either range. Failure is atomic. */
static inline bool mulib_compute_spirv_emit(uint32_t *w,size_t cap,size_t *n,uint16_t op,const uint32_t *operands,size_t count){if(!w||!n||cap>SIZE_MAX/sizeof *w||*n>cap||count>=65535||count+1>cap-*n||(!operands&&count))return false;size_t at=*n;if(count)memmove(w+at+1,operands,count*sizeof *w);w[at]=((uint32_t)(count+1)<<16)|op;*n=at+count+1;return true;}
#ifdef __cplusplus
}
#endif
#endif
