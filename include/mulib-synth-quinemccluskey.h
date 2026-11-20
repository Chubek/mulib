/* MuLIB: synth-quinemccluskey. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_QUINEMCCLUSKEY in exactly one implementation translation unit.
 * Profile: Exact small-function prime implicant cover with explicit search budget; up to six variables.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_QUINEMCCLUSKEY_H
#define MULIB_SYNTH_QUINEMCCLUSKEY_H
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
typedef struct {unsigned care,bits;} mulib_synth_cube;
static inline int mulib_synth_qm_search(const uint64_t *cover,size_t n,uint64_t missing,size_t *chosen,size_t depth,size_t *best,size_t *best_n,size_t *budget){if(!*budget)return 0;--*budget;if(!missing){if(depth<*best_n){*best_n=depth;memcpy(best,chosen,depth*sizeof(size_t));}return 1;}if(depth+1>=*best_n||depth==64)return 1;uint64_t bit=missing&(~missing+1);for(size_t i=0;i<n;i++)if(cover[i]&bit){chosen[depth]=i;if(!mulib_synth_qm_search(cover,n,missing&~cover[i],chosen,depth+1,best,best_n,budget))return 0;}return 1;}
int mulib_synth_quinemccluskey(unsigned variables,uint64_t truth,mulib_synth_cube *out,size_t cap,size_t *count,size_t budget);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_QUINEMCCLUSKEY) && !defined(MULIB_SYNTH_QUINEMCCLUSKEY_H_IMPLEMENTED)
#define MULIB_SYNTH_QUINEMCCLUSKEY_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_synth_quinemccluskey(unsigned variables,uint64_t truth,mulib_synth_cube *out,size_t cap,size_t *count,size_t budget){if(variables>6||!count)return 0;unsigned limit=1u<<variables;uint64_t mask=limit==64?UINT64_MAX:(UINT64_C(1)<<limit)-1;if(truth&~mask)return 0;if(!truth){*count=0;return 1;}mulib_synth_cube cubes[729];uint64_t cover[729];size_t n=0;for(unsigned care=0;care<limit;care++)for(unsigned bits=0;bits<limit;bits++)if(!(bits&~care)){uint64_t c=0;for(unsigned i=0;i<limit;i++)if((i&care)==bits)c|=UINT64_C(1)<<i;if(c&&(c&~truth)==0){int prime=1;for(unsigned j=0;j<variables;j++)if(care&(1u<<j)){unsigned cm=care&~(1u<<j),bm=bits&cm;uint64_t cc=0;for(unsigned k=0;k<limit;k++)if((k&cm)==bm)cc|=UINT64_C(1)<<k;if((cc&~truth)==0){prime=0;break;}}if(prime){cubes[n]=(mulib_synth_cube){care,bits};cover[n++]=c;}}}size_t chosen[64],best[64],best_n=65;int complete=mulib_synth_qm_search(cover,n,truth,chosen,0,best,&best_n,&budget);if(!complete||best_n>64||best_n>cap||!out)return 0;for(size_t i=0;i<best_n;i++)out[i]=cubes[best[i]];*count=best_n;return 1;}
#ifdef __cplusplus
}
#endif
#endif
