/* MuLIB: dsim-engine. C11 single-header kernel.
 * Define MULIB_IMPL_DSIM_ENGINE in exactly one implementation translation unit.
 * Profile: Stable four-state single-signal event queue; not full IEEE simulation-region semantics.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_DSIM_ENGINE_H
#define MULIB_DSIM_ENGINE_H
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
#include "mulib-dsim-signal.h"
#include "mulib-dsim-region.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t time,delta,serial;size_t signal;unsigned region,value;} mulib_dsim_sim_event;
typedef struct {mulib_dsim_sim_signal *signals;size_t signal_count;mulib_dsim_sim_event *events;size_t n,cap;uint64_t time,delta,serial;unsigned region;} mulib_dsim_sim_engine;
typedef int (*mulib_dsim_sim_callback)(void *,mulib_dsim_sim_engine *,const mulib_dsim_sim_event *);
static inline int mulib_dsim_sim_less(mulib_dsim_sim_event a,mulib_dsim_sim_event b){if(a.time!=b.time)return a.time<b.time;if(a.delta!=b.delta)return a.delta<b.delta;if(a.region!=b.region)return a.region<b.region;return a.serial<b.serial;}
int mulib_dsim_engine_schedule(mulib_dsim_sim_engine *s,uint64_t time,uint64_t delta,unsigned region,size_t signal,unsigned value);
int mulib_dsim_engine_run(mulib_dsim_sim_engine *s,uint64_t until,size_t budget,mulib_dsim_sim_callback callback,void *ctx,size_t *executed);
void mulib_dsim_engine_free(mulib_dsim_sim_engine *s);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DSIM_ENGINE) && !defined(MULIB_DSIM_ENGINE_H_IMPLEMENTED)
#define MULIB_DSIM_ENGINE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_dsim_engine_schedule(mulib_dsim_sim_engine *s,uint64_t time,uint64_t delta,unsigned region,size_t signal,unsigned value){if(!s||region>MULIB_DSIM_SIM_POSTPONED||signal>=s->signal_count||value>MULIB_DSIM_LOGIC_Z||time<s->time||(time==s->time&&(delta<s->delta||(delta==s->delta&&region<s->region)))||s->serial==UINT64_MAX)return 0;mulib_dsim_sim_event e={time,delta,s->serial,signal,region,value};if(!mulib_storage_push((void**)&s->events,&s->n,&s->cap,sizeof e,&e))return 0;s->serial++;size_t i=s->n-1;while(i){size_t p=(i-1)/2;if(!mulib_dsim_sim_less(s->events[i],s->events[p]))break;mulib_dsim_sim_event v=s->events[i];s->events[i]=s->events[p];s->events[p]=v;i=p;}return 1;}
int mulib_dsim_engine_run(mulib_dsim_sim_engine *s,uint64_t until,size_t budget,mulib_dsim_sim_callback callback,void *ctx,size_t *executed){if(!s||(!s->signals&&s->signal_count)||until<s->time)return 0;size_t count=0;while(s->n&&s->events[0].time<=until&&count<budget){mulib_dsim_sim_event e=s->events[0];s->events[0]=s->events[--s->n];size_t i=0;while(i<s->n/2){size_t l=2*i+1,r=l+1,j=r<s->n&&mulib_dsim_sim_less(s->events[r],s->events[l])?r:l;if(!mulib_dsim_sim_less(s->events[j],s->events[i]))break;mulib_dsim_sim_event v=s->events[i];s->events[i]=s->events[j];s->events[j]=v;i=j;}s->time=e.time;s->delta=e.delta;s->region=e.region;int change;if(!mulib_dsim_signal_set(s->signals+e.signal,e.value,e.time,&change))return 0;count++;if(change&&callback&&!callback(ctx,s,&e))return 0;}if(executed)*executed=count;return !s->n||s->events[0].time>until;}
void mulib_dsim_engine_free(mulib_dsim_sim_engine *s){if(s){free(s->events);s->events=NULL;s->n=s->cap=0;}}
#ifdef __cplusplus
}
#endif
#endif
