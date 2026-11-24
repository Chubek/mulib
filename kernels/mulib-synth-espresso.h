/* MuLIB: synth-espresso. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_ESPRESSO in exactly one implementation translation unit.
 * Profile: Bounded greedy two-level expand/cover heuristic (up to six inputs), not a full Espresso implementation.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_ESPRESSO_H
#define MULIB_SYNTH_ESPRESSO_H
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
#include "mulib-synth-quinemccluskey.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_synth_espresso(unsigned vars,uint64_t on,uint64_t dc,mulib_synth_cube *out,size_t cap,size_t *count);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_ESPRESSO) && !defined(MULIB_SYNTH_ESPRESSO_H_IMPLEMENTED)
#define MULIB_SYNTH_ESPRESSO_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_synth_espresso(unsigned vars,uint64_t on,uint64_t dc,mulib_synth_cube *out,size_t cap,size_t *count){if(vars>6||!count||(on&dc))return 0;unsigned limit=1u<<vars;uint64_t valid=limit==64?UINT64_MAX:(UINT64_C(1)<<limit)-1;if((on|dc)&~valid)return 0;size_t n=0;uint64_t left=on;while(left){unsigned x=0;while(!(left&(UINT64_C(1)<<x)))x++;unsigned care=limit-1,bits=x;for(unsigned j=0;j<vars;j++){unsigned m=care&~(1u<<j),b=bits&m;uint64_t cover=0;for(unsigned k=0;k<limit;k++)if((k&m)==b)cover|=UINT64_C(1)<<k;if(!(cover&~(on|dc))){care=m;bits=b;}}if(!out||n==cap)return 0;out[n++]=(mulib_synth_cube){care,bits};for(unsigned k=0;k<limit;k++)if((k&care)==bits)left&=~(UINT64_C(1)<<k);}*count=n;return 1;}
#ifdef __cplusplus
}
#endif
#endif
