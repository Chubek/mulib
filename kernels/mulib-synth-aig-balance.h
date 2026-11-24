/* MuLIB: synth-aig-balance. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_AIG_BALANCE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_AIG_BALANCE_H
#define MULIB_SYNTH_AIG_BALANCE_H
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
#include "mulib-synth-aig-rewrite.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_synth_aig_balance(mulib_synth_aig_graph *a,const size_t *literals,size_t n,size_t *root);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_AIG_BALANCE) && !defined(MULIB_SYNTH_AIG_BALANCE_H_IMPLEMENTED)
#define MULIB_SYNTH_AIG_BALANCE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_synth_aig_balance(mulib_synth_aig_graph *a,const size_t *literals,size_t n,size_t *root){if(!a||!root||(!literals&&n))return 0;if(!n){*root=1;return 1;}size_t *v=(size_t*)mulib_storage_alloc(n,sizeof *v);if(!v)return 0;memcpy(v,literals,n*sizeof *v);size_t original=a->n;while(n>1){size_t m=0,i=0;for(;i+1<n;i+=2)if(!mulib_synth_aig_graph_and(a,v[i],v[i+1],v+m++)){a->n=original;free(v);return 0;}if(i<n)v[m++]=v[i];n=m;}*root=v[0];free(v);return 1;}
#ifdef __cplusplus
}
#endif
#endif
