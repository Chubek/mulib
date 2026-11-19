/* MuLIB / Amb3r: dsim-cosim. C11 single-header kernel.
 * Define MULIB_IMPL_DSIM_COSIM in exactly one implementation translation unit.
 * Profile: Fixed maximum-step sequential orchestration; not conservative/optimistic mixed-signal synchronization.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_DSIM_COSIM_H
#define MULIB_DSIM_COSIM_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef int (*ma_cosim_step)(void *,uint64_t,uint64_t);
int mulib_dsim_cosim_run(uint64_t start,uint64_t end,uint64_t max_step,ma_cosim_step analog,ma_cosim_step digital,void *ctx,size_t budget);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DSIM_COSIM) && !defined(MULIB_DSIM_COSIM_H_IMPLEMENTED)
#define MULIB_DSIM_COSIM_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_dsim_cosim_run(uint64_t start,uint64_t end,uint64_t max_step,ma_cosim_step analog,ma_cosim_step digital,void *ctx,size_t budget){if(start>end||!max_step||!analog||!digital)return 0;uint64_t t=start;for(size_t i=0;t<end&&i<budget;i++){uint64_t next=t+MA_MIN(max_step,end-t);if(!analog(ctx,t,next)||!digital(ctx,t,next))return 0;t=next;}return t==end;}
#ifdef __cplusplus
}
#endif
#endif
