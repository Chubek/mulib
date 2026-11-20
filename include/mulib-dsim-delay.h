/* MuLIB: dsim-delay. C11 single-header kernel.
 * Define MULIB_IMPL_DSIM_DELAY in exactly one implementation translation unit.
 * Profile: Transport and single-driver-style inertial replacement; not full HDL transaction scheduling.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_DSIM_DELAY_H
#define MULIB_DSIM_DELAY_H
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
#include "mulib-dsim-engine.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_dsim_delay_transport(mulib_dsim_sim_engine *s,size_t signal,unsigned value,uint64_t delay);
int mulib_dsim_delay_inertial(mulib_dsim_sim_engine *s,size_t signal,unsigned value,uint64_t delay);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DSIM_DELAY) && !defined(MULIB_DSIM_DELAY_H_IMPLEMENTED)
#define MULIB_DSIM_DELAY_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_dsim_delay_transport(mulib_dsim_sim_engine *s,size_t signal,unsigned value,uint64_t delay){if(!s||s->time>UINT64_MAX-delay||(delay==0&&s->delta==UINT64_MAX))return 0;return mulib_dsim_engine_schedule(s,s->time+delay,delay?0:s->delta+1,MULIB_DSIM_SIM_ACTIVE,signal,value);}
int mulib_dsim_delay_inertial(mulib_dsim_sim_engine *s,size_t signal,unsigned value,uint64_t delay){if(!s||s->time>UINT64_MAX-delay||signal>=s->signal_count||value>MULIB_DSIM_LOGIC_Z||s->serial==UINT64_MAX||(delay==0&&s->delta==UINT64_MAX))return 0;if(!mulib_storage_reserve((void**)&s->events,&s->cap,s->n+1,sizeof *s->events))return 0;size_t n=0;for(size_t i=0;i<s->n;i++)if(s->events[i].signal!=signal)s->events[n++]=s->events[i];s->n=n;for(size_t k=n/2;k-->0;){size_t i=k;while(i<n/2){size_t l=2*i+1,r=l+1,j=r<n&&mulib_dsim_sim_less(s->events[r],s->events[l])?r:l;if(!mulib_dsim_sim_less(s->events[j],s->events[i]))break;mulib_dsim_sim_event t=s->events[i];s->events[i]=s->events[j];s->events[j]=t;i=j;}}return mulib_dsim_delay_transport(s,signal,value,delay);}
#ifdef __cplusplus
}
#endif
#endif
