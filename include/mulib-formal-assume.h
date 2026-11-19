/* MuLIB / Amb3r: formal-assume. C11 single-header kernel.
 * Define MULIB_IMPL_FORMAL_ASSUME in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FORMAL_ASSUME_H
#define MULIB_FORMAL_ASSUME_H
#include "mulib-amb3r-base.h"
#include "mulib-formal-transition.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_formal_assume_guarantee(const ma_transition *s,const unsigned char *assume,const unsigned char *guarantee,size_t *bad_state);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FORMAL_ASSUME) && !defined(MULIB_FORMAL_ASSUME_H_IMPLEMENTED)
#define MULIB_FORMAL_ASSUME_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_formal_assume_guarantee(const ma_transition *s,const unsigned char *assume,const unsigned char *guarantee,size_t *bad_state){if(!mulib_formal_transition_valid(s)||!assume||!guarantee)return -1;unsigned char *seen=(unsigned char*)ma_alloc(s->states,1);size_t *q=(size_t*)ma_alloc(s->states,sizeof *q);if(!seen||!q){free(seen);free(q);return -1;}size_t h=0,t=0;for(size_t i=0;i<s->states;i++)if(s->initial[i]&&assume[i]){seen[i]=1;q[t++]=i;}int r=1;while(h<t){size_t i=q[h++];if(!guarantee[i]){if(bad_state)*bad_state=i;r=0;break;}for(size_t j=s->offsets[i];j<s->offsets[i+1];j++){size_t v=s->targets[j];if(assume[v]&&!seen[v]){seen[v]=1;q[t++]=v;}}}free(seen);free(q);return r;}
#ifdef __cplusplus
}
#endif
#endif
