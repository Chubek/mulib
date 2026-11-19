/* MuLIB / Amb3r: dsim-clock. C11 single-header kernel.
 * Define MULIB_IMPL_DSIM_CLOCK in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_DSIM_CLOCK_H
#define MULIB_DSIM_CLOCK_H
#include "mulib-amb3r-base.h"
#include "mulib-dsim-engine.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_dsim_clock_schedule(ma_sim_engine *s,size_t signal,uint64_t start,uint64_t period,size_t cycles);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DSIM_CLOCK) && !defined(MULIB_DSIM_CLOCK_H_IMPLEMENTED)
#define MULIB_DSIM_CLOCK_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_dsim_clock_schedule(ma_sim_engine *s,size_t signal,uint64_t start,uint64_t period,size_t cycles){if(!s||signal>=s->signal_count||period<2||start<s->time||cycles>(UINT64_MAX-start)/period||cycles>(SIZE_MAX-s->n)/2)return 0;if(!ma_reserve((void**)&s->events,&s->cap,s->n+2*cycles,sizeof *s->events))return 0;for(size_t i=0;i<cycles;i++){uint64_t t=start+(uint64_t)i*period;if(!mulib_dsim_engine_schedule(s,t,t==s->time?s->delta+1:0,MA_SIM_ACTIVE,signal,MA_LOGIC_1)||!mulib_dsim_engine_schedule(s,t+period/2,0,MA_SIM_ACTIVE,signal,MA_LOGIC_0))return 0;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
