/* MuLIB / Amb3r: dsim-signal. C11 single-header kernel.
 * Define MULIB_IMPL_DSIM_SIGNAL in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_DSIM_SIGNAL_H
#define MULIB_DSIM_SIGNAL_H
#include "mulib-amb3r-base.h"
#include "mulib-dsim-resolve.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {unsigned value,previous;uint64_t time,changes;} ma_sim_signal;
int mulib_dsim_signal_set(ma_sim_signal *s,unsigned value,uint64_t time,int *changed);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DSIM_SIGNAL) && !defined(MULIB_DSIM_SIGNAL_H_IMPLEMENTED)
#define MULIB_DSIM_SIGNAL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_dsim_signal_set(ma_sim_signal *s,unsigned value,uint64_t time,int *changed){if(!s||value>MA_LOGIC_Z||time<s->time||!changed)return 0;*changed=s->value!=value;if(*changed){s->previous=s->value;s->value=value;s->time=time;if(s->changes!=UINT64_MAX)s->changes++;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
