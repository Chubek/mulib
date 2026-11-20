/* MuLIB: synth-aig-rewrite. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_AIG_REWRITE in exactly one implementation translation unit.
 * Profile: Dynamic AIG reconstruction with trivial Boolean simplification and structural sharing; no general cut-based rewrite library.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_AIG_REWRITE_H
#define MULIB_SYNTH_AIG_REWRITE_H
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
/* Independent dynamically sized AIG representation, literal 0=false, 1=true. */
typedef struct {size_t a,b;unsigned input;} mulib_synth_aig_graph_node;
typedef struct {mulib_synth_aig_graph_node *v;size_t n,cap;} mulib_synth_aig_graph;
static inline int mulib_synth_aig_graph_input(mulib_synth_aig_graph *a,size_t *out){if(!a||!out||a->n>=SIZE_MAX/2-1)return 0;mulib_synth_aig_graph_node n={0,0,1};if(!mulib_storage_push((void**)&a->v,&a->n,&a->cap,sizeof n,&n))return 0;*out=2*a->n;return 1;}
static inline int mulib_synth_aig_graph_and(mulib_synth_aig_graph *g,size_t a,size_t b,size_t *out){if(!g||!out||a/2>g->n||b/2>g->n)return 0;if(!a||!b||a==(b^1)){*out=0;return 1;}if(a==1||a==b){*out=b;return 1;}if(b==1){*out=a;return 1;}if(a>b){size_t t=a;a=b;b=t;}for(size_t i=0;i<g->n;i++)if(!g->v[i].input&&g->v[i].a==a&&g->v[i].b==b){*out=2*(i+1);return 1;}if(g->n>=SIZE_MAX/2-1)return 0;mulib_synth_aig_graph_node n={a,b,0};if(!mulib_storage_push((void**)&g->v,&g->n,&g->cap,sizeof n,&n))return 0;*out=2*g->n;return 1;}
int mulib_synth_aig_rewrite(const mulib_synth_aig_graph *a,mulib_synth_aig_graph *out,size_t *mapping);
void mulib_synth_aig_rewrite_free(mulib_synth_aig_graph *a);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_AIG_REWRITE) && !defined(MULIB_SYNTH_AIG_REWRITE_H_IMPLEMENTED)
#define MULIB_SYNTH_AIG_REWRITE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_synth_aig_rewrite(const mulib_synth_aig_graph *a,mulib_synth_aig_graph *out,size_t *mapping){if(!a||!out||out->v||(!mapping&&a->n))return 0;mulib_synth_aig_graph t={0};for(size_t i=0;i<a->n;i++){mulib_synth_aig_graph_node n=a->v[i];size_t lit;if(n.input){if(!mulib_synth_aig_graph_input(&t,&lit))goto fail;}else{if(n.a/2>i||n.b/2>i)goto fail;size_t x=n.a<2?n.a:mapping[n.a/2-1]^(n.a&1),y=n.b<2?n.b:mapping[n.b/2-1]^(n.b&1);if(!mulib_synth_aig_graph_and(&t,x,y,&lit))goto fail;}mapping[i]=lit;}*out=t;return 1;fail:free(t.v);return 0;}
void mulib_synth_aig_rewrite_free(mulib_synth_aig_graph *a){if(a){free(a->v);memset(a,0,sizeof *a);}}
#ifdef __cplusplus
}
#endif
#endif
