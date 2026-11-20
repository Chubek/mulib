/* MuLIB: synth-retime. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_RETIME in exactly one implementation translation unit.
 * Profile: Validate/apply supplied integer retiming labels; no optimal label search.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_RETIME_H
#define MULIB_SYNTH_RETIME_H
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
typedef struct {size_t from,to;uint64_t registers;} mulib_synth_retime_edge;
int mulib_synth_retime(size_t n,const mulib_synth_retime_edge *edges,size_t m,const int64_t *r,uint64_t *weights);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_RETIME) && !defined(MULIB_SYNTH_RETIME_H_IMPLEMENTED)
#define MULIB_SYNTH_RETIME_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_synth_retime(size_t n,const mulib_synth_retime_edge *edges,size_t m,const int64_t *r,uint64_t *weights){if((!edges&&m)||(!r&&n)||(!weights&&m))return 0;for(size_t i=0;i<m;i++){size_t u=edges[i].from,v=edges[i].to;if(u>=n||v>=n)return 0;uint64_t w=edges[i].registers;if(r[v]>=r[u]){uint64_t d=(uint64_t)r[v]-(uint64_t)r[u];if(w>UINT64_MAX-d)return 0;weights[i]=w+d;}else{uint64_t d=(uint64_t)r[u]-(uint64_t)r[v];if(w<d)return 0;weights[i]=w-d;}}return 1;}
#ifdef __cplusplus
}
#endif
#endif
