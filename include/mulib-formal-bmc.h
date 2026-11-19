/* MuLIB / Amb3r: formal-bmc. C11 single-header kernel.
 * Define MULIB_IMPL_FORMAL_BMC in exactly one implementation translation unit.
 * Profile: Explicit-state bounded safety check; not SAT-unrolled symbolic BMC.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FORMAL_BMC_H
#define MULIB_FORMAL_BMC_H
#include "mulib-amb3r-base.h"
#include "mulib-formal-transition.h"
#include "mulib-formal-counterexample.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_formal_bmc(const ma_transition *s,const unsigned char *bad,size_t bound,size_t *witness,size_t capacity,size_t *length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FORMAL_BMC) && !defined(MULIB_FORMAL_BMC_H_IMPLEMENTED)
#define MULIB_FORMAL_BMC_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_formal_bmc(const ma_transition *s,const unsigned char *bad,size_t bound,size_t *witness,size_t capacity,size_t *length){if(!mulib_formal_transition_valid(s)||!bad||!length)return -1;unsigned char *seen=(unsigned char*)ma_alloc(s->states,1);size_t *parent=(size_t*)ma_alloc(s->states,sizeof *parent),*dist=(size_t*)ma_alloc(s->states,sizeof *dist);if(!seen||!parent||!dist){free(seen);free(parent);free(dist);return -1;}int r=-1;if(mulib_formal_transition_reachable(s,seen,parent,dist)){size_t best=MA_NONE;for(size_t i=0;i<s->states;i++)if(seen[i]&&bad[i]&&dist[i]<=bound&&(best==MA_NONE||dist[i]<dist[best]))best=i;if(best==MA_NONE){*length=0;r=1;}else{r=mulib_formal_counterexample(parent,s->states,best,witness,capacity,length)?0:-1;}}free(seen);free(parent);free(dist);return r;}
#ifdef __cplusplus
}
#endif
#endif
