/* MuLIB: pcb-ratsnest. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_RATSNEST in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_RATSNEST_H
#define MULIB_PCB_RATSNEST_H
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

size_t mulib_pcb_ratsnest(const mulib_geom2_point *p,const uint64_t *net,size_t n,mulib_graph_weighted_edge *out,size_t cap);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_RATSNEST) && !defined(MULIB_PCB_RATSNEST_H_IMPLEMENTED)
#define MULIB_PCB_RATSNEST_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_pcb_ratsnest(const mulib_geom2_point *p,const uint64_t *net,size_t n,mulib_graph_weighted_edge *out,size_t cap){if((!p&&n)||(!net&&n))return MULIB_STORAGE_NONE;size_t *parent=(size_t*)mulib_storage_alloc(n,sizeof(size_t));if(!parent)return MULIB_STORAGE_NONE;for(size_t i=0;i<n;i++)parent[i]=i;size_t count=0;while(1){size_t a=MULIB_STORAGE_NONE,b=MULIB_STORAGE_NONE;double best=INFINITY;for(size_t i=0;i<n;i++)for(size_t j=i+1;j<n;j++)if(net[i]&&net[i]==net[j]&&mulib_graph_set_find(parent,i)!=mulib_graph_set_find(parent,j)){double d=mulib_geom2_distance(p[i],p[j]);if(d<best){a=i;b=j;best=d;}}if(a==MULIB_STORAGE_NONE)break;mulib_graph_set_union(parent,a,b);if(out&&count<cap)out[count]=(mulib_graph_weighted_edge){a,b,best};count++;}free(parent);return count;}
#ifdef __cplusplus
}
#endif
#endif
