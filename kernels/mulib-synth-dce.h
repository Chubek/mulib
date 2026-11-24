/* MuLIB: synth-dce. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_DCE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_DCE_H
#define MULIB_SYNTH_DCE_H
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

int mulib_synth_dce(const mulib_synth_aig_graph *g,const size_t *roots,size_t n,mulib_synth_aig_graph *out,size_t *mapping);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_DCE) && !defined(MULIB_SYNTH_DCE_H_IMPLEMENTED)
#define MULIB_SYNTH_DCE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_synth_dce(const mulib_synth_aig_graph *g,const size_t *roots,size_t n,mulib_synth_aig_graph *out,size_t *mapping){if(!g||!out||out->v||(!roots&&n)||(!mapping&&g->n))return 0;unsigned char *live=(unsigned char*)mulib_storage_alloc(g->n+1,1);if(!live)return 0;mulib_synth_aig_graph t={0};for(size_t i=0;i<n;i++){if(roots[i]/2>g->n)goto fail;live[roots[i]/2]=1;}for(size_t i=g->n;i;i--)if(live[i]&&!g->v[i-1].input){mulib_synth_aig_graph_node x=g->v[i-1];if(x.a/2>=i||x.b/2>=i)goto fail;live[x.a/2]=live[x.b/2]=1;}for(size_t i=1;i<=g->n;i++){mapping[i-1]=MULIB_STORAGE_NONE;if(!live[i])continue;mulib_synth_aig_graph_node x=g->v[i-1];if(x.input){if(!mulib_synth_aig_graph_input(&t,mapping+i-1))goto fail;}else{size_t a=x.a<2?x.a:mapping[x.a/2-1]^(x.a&1),b=x.b<2?x.b:mapping[x.b/2-1]^(x.b&1);if(!mulib_synth_aig_graph_and(&t,a,b,mapping+i-1))goto fail;}}free(live);*out=t;return 1;fail:free(live);free(t.v);return 0;}
#ifdef __cplusplus
}
#endif
#endif
