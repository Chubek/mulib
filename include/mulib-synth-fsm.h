/* MuLIB / Amb3r: synth-fsm. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_FSM in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_FSM_H
#define MULIB_SYNTH_FSM_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_synth_fsm_minimize(size_t states,size_t alphabet,const size_t *next,const uint64_t *output,size_t *classes,size_t *class_count);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_FSM) && !defined(MULIB_SYNTH_FSM_H_IMPLEMENTED)
#define MULIB_SYNTH_FSM_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_synth_fsm_minimize(size_t states,size_t alphabet,const size_t *next,const uint64_t *output,size_t *classes,size_t *class_count){size_t total;if(!states||!alphabet||!ma_mul(states,alphabet,&total)||!next||!output||!classes||!class_count)return 0;for(size_t i=0;i<total;i++)if(next[i]>=states)return 0;size_t *old=(size_t*)ma_alloc(states,sizeof *old);if(!old)return 0;size_t n=0;for(size_t i=0;i<states;i++){size_t j=0;while(j<i&&output[j]!=output[i])j++;classes[i]=j<i?classes[j]:n++;}for(size_t k=0;k<states;k++){memcpy(old,classes,states*sizeof *old);size_t m=0;for(size_t i=0;i<states;i++){size_t j=0;for(;j<i;j++){if(old[i]!=old[j])continue;size_t a=0;while(a<alphabet&&old[next[i*alphabet+a]]==old[next[j*alphabet+a]])a++;if(a==alphabet)break;}classes[i]=j<i?classes[j]:m++;}if(m==n){*class_count=m;free(old);return 1;}n=m;}free(old);return 0;}
#ifdef __cplusplus
}
#endif
#endif
