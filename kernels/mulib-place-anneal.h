/* MuLIB: place-anneal. C11 single-header kernel.
 * Define MULIB_IMPL_PLACE_ANNEAL in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PLACE_ANNEAL_H
#define MULIB_PLACE_ANNEAL_H
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
#include "mulib-geom2-core.h"
#include "mulib-graph-core.h"
#ifdef __cplusplus
extern "C" {
#endif
static inline double mulib_place_cost(const mulib_geom2_point *s,const mulib_graph_weighted_edge *e,size_t m,const size_t *a){double cost=0;for(size_t i=0;i<m;i++){mulib_geom2_point p=s[a[e[i].from]],q=s[a[e[i].to]];cost+=e[i].weight*(fabs(p.x-q.x)+fabs(p.y-q.y));}return cost;}
int mulib_place_anneal(const mulib_geom2_point *sites,size_t n,const mulib_graph_weighted_edge *nets,size_t m,size_t *assignment,mulib_num_rng *rng,size_t iterations,double initial_temp,double cooling);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PLACE_ANNEAL) && !defined(MULIB_PLACE_ANNEAL_H_IMPLEMENTED)
#define MULIB_PLACE_ANNEAL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_place_anneal(const mulib_geom2_point *sites,size_t n,const mulib_graph_weighted_edge *nets,size_t m,size_t *assignment,mulib_num_rng *rng,size_t iterations,double initial_temp,double cooling){if(!sites||!n||!assignment||!rng||(!nets&&m)||initial_temp<=0||cooling<=0||cooling>1)return 0;for(size_t i=0;i<n;i++){if(assignment[i]>=n||!mulib_geom2_point_valid(sites[i]))return 0;for(size_t j=0;j<i;j++)if(assignment[i]==assignment[j])return 0;}for(size_t i=0;i<m;i++)if(nets[i].from>=n||nets[i].to>=n||nets[i].weight<0||!isfinite(nets[i].weight))return 0;size_t *best=(size_t*)mulib_storage_alloc(n,sizeof *best);if(!best)return 0;memcpy(best,assignment,n*sizeof *best);double cost=mulib_place_cost(sites,nets,m,assignment),best_cost=cost,temp=initial_temp;for(size_t k=0;k<iterations;k++){size_t a=mulib_num_rng_next(rng)%n,b=mulib_num_rng_next(rng)%n;if(a==b)continue;size_t t=assignment[a];assignment[a]=assignment[b];assignment[b]=t;double next=mulib_place_cost(sites,nets,m,assignment);if(next<cost||(temp>DBL_MIN&&mulib_num_rng_uniform(rng)<exp((cost-next)/temp))){cost=next;if(cost<best_cost){best_cost=cost;memcpy(best,assignment,n*sizeof *best);}}else{t=assignment[a];assignment[a]=assignment[b];assignment[b]=t;}temp*=cooling;}memcpy(assignment,best,n*sizeof *best);free(best);return 1;}
#ifdef __cplusplus
}
#endif
#endif
