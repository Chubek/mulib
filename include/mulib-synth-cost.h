/* MuLIB / Amb3r: synth-cost. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_COST in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_COST_H
#define MULIB_SYNTH_COST_H
#include "mulib-amb3r-base.h"
#include "mulib-synth-aig-rewrite.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_synth_cost(const ma_aig *g,double gate_area,double gate_delay,double *area,double *delay);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_COST) && !defined(MULIB_SYNTH_COST_H_IMPLEMENTED)
#define MULIB_SYNTH_COST_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_synth_cost(const ma_aig *g,double gate_area,double gate_delay,double *area,double *delay){if(!g||!area||!delay||gate_area<0||gate_delay<0)return 0;size_t *depth=(size_t*)ma_alloc(g->n+1,sizeof *depth);if(!depth)return 0;size_t count=0,max=0;for(size_t i=1;i<=g->n;i++){ma_aig_node n=g->v[i-1];if(!n.input){if(n.a/2>=i||n.b/2>=i){free(depth);return 0;}depth[i]=1+MA_MAX(depth[n.a/2],depth[n.b/2]);max=MA_MAX(max,depth[i]);count++;}}*area=count*gate_area;*delay=max*gate_delay;free(depth);return isfinite(*area)&&isfinite(*delay);}
#ifdef __cplusplus
}
#endif
#endif
