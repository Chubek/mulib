/* MuLIB: formal-kinduction. C11 single-header kernel.
 * Define MULIB_IMPL_FORMAL_KINDUCTION in exactly one implementation translation unit.
 * Profile: Explicit-state inductive step check; base case must be checked separately.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FORMAL_KINDUCTION_H
#define MULIB_FORMAL_KINDUCTION_H
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
#include "mulib-num-core.h"
#include "mulib-graph-core.h"
#include "mulib-formal-transition.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_formal_kinduction_step(const mulib_formal_transition *s,const unsigned char *good,size_t k);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FORMAL_KINDUCTION) && !defined(MULIB_FORMAL_KINDUCTION_H_IMPLEMENTED)
#define MULIB_FORMAL_KINDUCTION_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_formal_kinduction_step(const mulib_formal_transition *s,const unsigned char *good,size_t k){if(!mulib_formal_transition_valid(s)||!good||!k)return -1;unsigned char *a=(unsigned char*)mulib_storage_alloc(s->states,1),*b=(unsigned char*)mulib_storage_alloc(s->states,1);if(!a||!b){free(a);free(b);return -1;}for(size_t i=0;i<s->states;i++)a[i]=!!good[i];int r=1;for(size_t step=1;step<=k;step++){memset(b,0,s->states);for(size_t i=0;i<s->states;i++)if(a[i])for(size_t j=s->offsets[i];j<s->offsets[i+1];j++){size_t v=s->targets[j];if(step==k&&!good[v]){r=0;goto end;}if(good[v])b[v]=1;}unsigned char *t=a;a=b;b=t;if(!memcmp(a,b,s->states)&&step<k)step=k-1;if(step==k)break;}end:free(a);free(b);return r;}
#ifdef __cplusplus
}
#endif
#endif
