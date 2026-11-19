/* MuLIB / Amb3r: pcb-ratsnest. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_RATSNEST in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_RATSNEST_H
#define MULIB_PCB_RATSNEST_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

size_t mulib_pcb_ratsnest(const ma_point *p,const uint64_t *net,size_t n,ma_edge *out,size_t cap);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_RATSNEST) && !defined(MULIB_PCB_RATSNEST_H_IMPLEMENTED)
#define MULIB_PCB_RATSNEST_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_pcb_ratsnest(const ma_point *p,const uint64_t *net,size_t n,ma_edge *out,size_t cap){if((!p&&n)||(!net&&n))return MA_NONE;size_t *parent=(size_t*)ma_alloc(n,sizeof(size_t));if(!parent)return MA_NONE;for(size_t i=0;i<n;i++)parent[i]=i;size_t count=0;while(1){size_t a=MA_NONE,b=MA_NONE;double best=INFINITY;for(size_t i=0;i<n;i++)for(size_t j=i+1;j<n;j++)if(net[i]&&net[i]==net[j]&&ma_find(parent,i)!=ma_find(parent,j)){double d=ma_dist(p[i],p[j]);if(d<best){a=i;b=j;best=d;}}if(a==MA_NONE)break;ma_union(parent,a,b);if(out&&count<cap)out[count]=(ma_edge){a,b,best};count++;}free(parent);return count;}
#ifdef __cplusplus
}
#endif
#endif
