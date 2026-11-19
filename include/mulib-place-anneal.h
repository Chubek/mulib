/* MuLIB / Amb3r: place-anneal. C11 single-header kernel.
 * Define MULIB_IMPL_PLACE_ANNEAL in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PLACE_ANNEAL_H
#define MULIB_PLACE_ANNEAL_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
static inline double ma_place_cost(const ma_point *s,const ma_edge *e,size_t m,const size_t *a){double cost=0;for(size_t i=0;i<m;i++){ma_point p=s[a[e[i].from]],q=s[a[e[i].to]];cost+=e[i].weight*(fabs(p.x-q.x)+fabs(p.y-q.y));}return cost;}
int mulib_place_anneal(const ma_point *sites,size_t n,const ma_edge *nets,size_t m,size_t *assignment,ma_rng *rng,size_t iterations,double initial_temp,double cooling);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PLACE_ANNEAL) && !defined(MULIB_PLACE_ANNEAL_H_IMPLEMENTED)
#define MULIB_PLACE_ANNEAL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_place_anneal(const ma_point *sites,size_t n,const ma_edge *nets,size_t m,size_t *assignment,ma_rng *rng,size_t iterations,double initial_temp,double cooling){if(!sites||!n||!assignment||!rng||(!nets&&m)||initial_temp<=0||cooling<=0||cooling>1)return 0;for(size_t i=0;i<n;i++){if(assignment[i]>=n||!ma_point_valid(sites[i]))return 0;for(size_t j=0;j<i;j++)if(assignment[i]==assignment[j])return 0;}for(size_t i=0;i<m;i++)if(nets[i].from>=n||nets[i].to>=n||nets[i].weight<0||!isfinite(nets[i].weight))return 0;size_t *best=(size_t*)ma_alloc(n,sizeof *best);if(!best)return 0;memcpy(best,assignment,n*sizeof *best);double cost=ma_place_cost(sites,nets,m,assignment),best_cost=cost,temp=initial_temp;for(size_t k=0;k<iterations;k++){size_t a=ma_rand(rng)%n,b=ma_rand(rng)%n;if(a==b)continue;size_t t=assignment[a];assignment[a]=assignment[b];assignment[b]=t;double next=ma_place_cost(sites,nets,m,assignment);if(next<cost||(temp>DBL_MIN&&ma_uniform(rng)<exp((cost-next)/temp))){cost=next;if(cost<best_cost){best_cost=cost;memcpy(best,assignment,n*sizeof *best);}}else{t=assignment[a];assignment[a]=assignment[b];assignment[b]=t;}temp*=cooling;}memcpy(assignment,best,n*sizeof *best);free(best);return 1;}
#ifdef __cplusplus
}
#endif
#endif
