/* MuLIB: synth-bdd. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_BDD in exactly one implementation translation unit.
 * Profile: Reduced ordered BDD from truth tables up to six variables.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_BDD_H
#define MULIB_SYNTH_BDD_H
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
typedef struct {unsigned var;size_t low,high;} mulib_synth_bdd_node;
typedef struct {mulib_synth_bdd_node *nodes;size_t n,cap;} mulib_synth_bdd;
int mulib_synth_bdd_build(mulib_synth_bdd *b,unsigned variables,uint64_t truth,size_t *root);
int mulib_synth_bdd_eval(const mulib_synth_bdd *b,size_t root,uint64_t input,int *out);
void mulib_synth_bdd_free(mulib_synth_bdd *b);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_BDD) && !defined(MULIB_SYNTH_BDD_H_IMPLEMENTED)
#define MULIB_SYNTH_BDD_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_synth_bdd_build(mulib_synth_bdd *b,unsigned variables,uint64_t truth,size_t *root){if(!b||b->nodes||!root||variables>6)return 0;mulib_synth_bdd t={0};mulib_synth_bdd_node z={UINT_MAX,0,0};if(!mulib_storage_push((void**)&t.nodes,&t.n,&t.cap,sizeof z,&z)||!mulib_storage_push((void**)&t.nodes,&t.n,&t.cap,sizeof z,&z)){free(t.nodes);return 0;}size_t ids[64];for(size_t i=0;i<((size_t)1<<variables);i++)ids[i]=(truth>>i)&1;for(unsigned v=variables;v-->0;){size_t half=(size_t)1<<v;for(size_t i=0;i<half;i++){size_t lo=ids[i],hi=ids[i+half];if(lo==hi){ids[i]=lo;continue;}size_t j=2;for(;j<t.n;j++)if(t.nodes[j].var==v&&t.nodes[j].low==lo&&t.nodes[j].high==hi)break;if(j==t.n){mulib_synth_bdd_node q={v,lo,hi};if(!mulib_storage_push((void**)&t.nodes,&t.n,&t.cap,sizeof q,&q)){free(t.nodes);return 0;}}ids[i]=j;}}*root=ids[0];*b=t;return 1;}
int mulib_synth_bdd_eval(const mulib_synth_bdd *b,size_t root,uint64_t input,int *out){if(!b||!out||root>=b->n)return 0;for(size_t i=0;root>1&&i<b->n;i++){mulib_synth_bdd_node q=b->nodes[root];if(q.var>=64||q.low>=b->n||q.high>=b->n)return 0;root=(input>>q.var)&1?q.high:q.low;}if(root>1)return 0;*out=(int)root;return 1;}
void mulib_synth_bdd_free(mulib_synth_bdd *b){if(b){free(b->nodes);memset(b,0,sizeof *b);}}
#ifdef __cplusplus
}
#endif
#endif
