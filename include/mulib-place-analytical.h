/* MuLIB / Amb3r: place-analytical. C11 single-header kernel.
 * Define MULIB_IMPL_PLACE_ANALYTICAL in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PLACE_ANALYTICAL_H
#define MULIB_PLACE_ANALYTICAL_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_place_analytical(ma_point *p,const unsigned char *fixed,size_t n,const ma_edge *e,size_t m,size_t iterations,double tolerance);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PLACE_ANALYTICAL) && !defined(MULIB_PLACE_ANALYTICAL_H_IMPLEMENTED)
#define MULIB_PLACE_ANALYTICAL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_place_analytical(ma_point *p,const unsigned char *fixed,size_t n,const ma_edge *e,size_t m,size_t iterations,double tolerance){if(!p||!fixed||!n||(!e&&m)||tolerance<=0)return 0;for(size_t i=0;i<m;i++)if(e[i].from>=n||e[i].to>=n||e[i].weight<0||!isfinite(e[i].weight))return 0;ma_point *q=(ma_point*)ma_alloc(n,sizeof *q);if(!q)return 0;int ok=0;for(size_t k=0;k<iterations;k++){double change=0;for(size_t i=0;i<n;i++){q[i]=p[i];if(fixed[i])continue;double x=0,y=0,w=0;for(size_t j=0;j<m;j++){size_t other=e[j].from==i?e[j].to:e[j].to==i?e[j].from:MA_NONE;if(other!=MA_NONE){x+=p[other].x*e[j].weight;y+=p[other].y*e[j].weight;w+=e[j].weight;}}if(w){q[i]=(ma_point){x/w,y/w};change=MA_MAX(change,ma_dist(p[i],q[i]));}}memcpy(p,q,n*sizeof *p);if(change<=tolerance){ok=1;break;}}free(q);return ok;}
#ifdef __cplusplus
}
#endif
#endif
