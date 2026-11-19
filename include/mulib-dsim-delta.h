/* MuLIB / Amb3r: dsim-delta. C11 single-header kernel.
 * Define MULIB_IMPL_DSIM_DELTA in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_DSIM_DELTA_H
#define MULIB_DSIM_DELTA_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef int (*ma_delta_function)(void *,const unsigned *,unsigned *,size_t);
int mulib_dsim_delta_settle(unsigned *signals,size_t n,ma_delta_function evaluate,void *ctx,size_t limit,size_t *cycles);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DSIM_DELTA) && !defined(MULIB_DSIM_DELTA_H_IMPLEMENTED)
#define MULIB_DSIM_DELTA_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_dsim_delta_settle(unsigned *signals,size_t n,ma_delta_function evaluate,void *ctx,size_t limit,size_t *cycles){if((!signals&&n)||!evaluate)return 0;unsigned *next=(unsigned*)ma_alloc(n,sizeof *next);if(!next)return 0;for(size_t k=0;k<limit;k++){if(!evaluate(ctx,signals,next,n)){free(next);return 0;}if(!memcmp(signals,next,n*sizeof *next)){if(cycles)*cycles=k+1;free(next);return 1;}memcpy(signals,next,n*sizeof *next);}free(next);if(cycles)*cycles=limit;return 0;}
#ifdef __cplusplus
}
#endif
#endif
