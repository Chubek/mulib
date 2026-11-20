/* MuLIB: formal-pdr. C11 single-header kernel.
 * Define MULIB_IMPL_FORMAL_PDR in exactly one implementation translation unit.
 * Profile: Explicit predecessor closure to compute an inductive invariant; not symbolic IC3/PDR.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FORMAL_PDR_H
#define MULIB_FORMAL_PDR_H
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
#include "mulib-formal-transition.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_formal_pdr(const mulib_formal_transition *s,const unsigned char *bad,unsigned char *invariant);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FORMAL_PDR) && !defined(MULIB_FORMAL_PDR_H_IMPLEMENTED)
#define MULIB_FORMAL_PDR_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_formal_pdr(const mulib_formal_transition *s,const unsigned char *bad,unsigned char *invariant){if(!mulib_formal_transition_valid(s)||!bad||!invariant)return -1;for(size_t i=0;i<s->states;i++)invariant[i]=!bad[i];int change=1;while(change){change=0;for(size_t i=0;i<s->states;i++)if(invariant[i])for(size_t k=s->offsets[i];k<s->offsets[i+1];k++)if(!invariant[s->targets[k]]){invariant[i]=0;change=1;break;}}for(size_t i=0;i<s->states;i++)if(s->initial[i]&&!invariant[i])return 0;return 1;}
#ifdef __cplusplus
}
#endif
#endif
