/* MuLIB / Amb3r: synth-egraph. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_EGRAPH in exactly one implementation translation unit.
 * Profile: Explicit equality congruence closure on AND expressions; not full equality saturation or integration with mulib-egraph.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_EGRAPH_H
#define MULIB_SYNTH_EGRAPH_H
#include "mulib-amb3r-base.h"
#include "mulib-synth-aig-rewrite.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_synth_egraph(const ma_aig *g,const ma_edge *equations,size_t n,size_t *classes);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_EGRAPH) && !defined(MULIB_SYNTH_EGRAPH_H_IMPLEMENTED)
#define MULIB_SYNTH_EGRAPH_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_synth_egraph(const ma_aig *g,const ma_edge *equations,size_t n,size_t *classes){if(!g||(!equations&&n)||(!classes&&g->n))return 0;for(size_t i=0;i<n;i++)if(equations[i].from>=g->n||equations[i].to>=g->n)return 0;for(size_t i=0;i<g->n;i++){classes[i]=i;if(!g->v[i].input&&(g->v[i].a/2>i||g->v[i].b/2>i))return 0;}for(size_t i=0;i<n;i++)ma_union(classes,equations[i].from,equations[i].to);int change=1;while(change){change=0;for(size_t i=0;i<g->n;i++)if(!g->v[i].input)for(size_t j=0;j<i;j++)if(!g->v[j].input&&ma_find(classes,i)!=ma_find(classes,j)){size_t x[2]={g->v[i].a,g->v[i].b},y[2]={g->v[j].a,g->v[j].b};for(unsigned k=0;k<2;k++){if(x[k]>=2)x[k]=2*(ma_find(classes,x[k]/2-1)+1)+(x[k]&1);if(y[k]>=2)y[k]=2*(ma_find(classes,y[k]/2-1)+1)+(y[k]&1);}if((x[0]==y[0]&&x[1]==y[1])||(x[0]==y[1]&&x[1]==y[0])){ma_union(classes,i,j);change=1;}}}for(size_t i=0;i<g->n;i++)classes[i]=ma_find(classes,i);return 1;}
#ifdef __cplusplus
}
#endif
#endif
