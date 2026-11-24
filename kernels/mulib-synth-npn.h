/* MuLIB: synth-npn. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_NPN in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_NPN_H
#define MULIB_SYNTH_NPN_H
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
#ifdef __cplusplus
extern "C" {
#endif

uint64_t mulib_synth_npn_canonical(unsigned n,uint64_t truth);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_NPN) && !defined(MULIB_SYNTH_NPN_H_IMPLEMENTED)
#define MULIB_SYNTH_NPN_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
uint64_t mulib_synth_npn_canonical(unsigned n,uint64_t truth){if(n>6)return 0;unsigned m=1u<<n;uint64_t mask=m==64?UINT64_MAX:(UINT64_C(1)<<m)-1,best=mask;unsigned p[6]={0,1,2,3,4,5};int done=0;while(!done){for(unsigned inv=0;inv<m;inv++){uint64_t t=0;for(unsigned x=0;x<m;x++){unsigned y=0;for(unsigned j=0;j<n;j++)y|=(((x^inv)>>j)&1u)<<p[j];t|=((truth>>y)&1u)<<x;}if(t<best)best=t;if((t^mask)<best)best=t^mask;}int i=(int)n-2;while(i>=0&&p[i]>=p[i+1])i--;if(i<0){done=1;continue;}int j=(int)n-1;while(p[j]<=p[i])j--;unsigned t=p[i];p[i]=p[j];p[j]=t;for(int a=i+1,b=(int)n-1;a<b;a++,b--){t=p[a];p[a]=p[b];p[b]=t;}}return best;}
#ifdef __cplusplus
}
#endif
#endif
